#ifndef SOCKET_H
#define SOCKET_H

#include "pleg.h"
using namespace Pleg;
#include "tao_forward.h"
using namespace tao;
#include <memory>
#include <string>
#include <list>
using namespace std;
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_pointer.hpp>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
using namespace boost;
#include "byte_array.h"
#include "glib.h"
#include "metatypes.h"

#define verbs (\
    NONE,\
    HEAD,\
    GET,\
    POST,\
    PATCH,\
    CATCH,\
    OPTIONS \
)
ENUM(verbs_type,verbs)

namespace drumlin {

/**
 * @brief The SocketFlushBehaviours enum
 * Coalesce - concatenate the buffers
 * Flush - remove buffers after processing
 * Sort - apply the handler sort function
 * CoalesceOnly - == Coalesce
 * CoalesceAndFlush - Coalesce & Flush
 * SortAndCoalesce - Sort & Coalesce
 * Everything - Sort & Coalesce & Flush
 */
enum SocketFlushBehaviours {
    Coalesce     = 1,
    Flush        = 2,
    Sort         = 4,
    CoalesceOnly = 1,
    CoalesceAndFlush = 3,
    SortAndCoalesce = 5,
    Everything   = 7,
};

/*
 * Forward declarations
 */

template <class Protocol>
class Socket;

/**
 * @brief The SocketHandler class : abstract class to generalize over sockets
 */
template <class Protocol>
class SocketHandler {
public:
    typedef drumlin::Socket<Protocol> Socket;
    virtual bool processTransmission(Socket *socket)=0;
    virtual bool receivePacket(Socket *socket)=0;
    virtual bool readyProcess(Socket *socket)=0;
    virtual bool reply(Socket *socket)=0;
    virtual void completing(Socket *socket, gint64 written)=0;
    virtual void sort(Socket *socket,drumlin::buffers_type &buffers)=0;
    virtual void disconnected(Socket *socket)=0;
};

#define READLOCK  lock_guard<recursive_mutex> l1(read_buffer_mutex);
#define WRITELOCK lock_guard<recursive_mutex> l2(write_buffer_mutex);

/**
 * @brief The Socket class
 */
template <class Protocol = asio::ip::tcp>
class Socket : public Object
{
public:
    typedef SocketHandler<Protocol> Handler;
    typedef typename Protocol::socket socket_type;
    Socket(boost::asio::io_service &io_service,Object *parent = 0,Handler *_handler = 0):Object(parent),handler(_handler),m_io_service(io_service),m_sock_type(io_service)
    {
        READLOCK
        WRITELOCK
        readBuffers.clear();
        writeBuffers.clear();
    }
    ~Socket()
    {
        READLOCK
        WRITELOCK
        writeBuffers.erase(std::remove_if(writeBuffers.begin(),writeBuffers.end(),[](auto &){return true;}),writeBuffers.end());
        readBuffers.erase(std::remove_if(readBuffers.begin(),readBuffers.end(),[](auto &){return true;}),readBuffers.end());
    }
    /**
     * @brief setTag : associate a void* with the socket
     * @param _tag void*
     */
    void setTag(void *_tag){tag = (void*)_tag;}
    /**
     * @brief getTag : return the void* associated with the socket
     * @return void*
     */
    void *getTag(){ return tag; }
    bool connectToHost(string host,string port)
    {
        typedef Protocol protocol_type;
        typedef typename protocol_type::resolver resolver_type;
        resolver_type resolver(m_io_service);
        typename protocol_type::resolver::query query(host,port);
        typename resolver_type::iterator iter = resolver.resolve(query);
        typename resolver_type::iterator end;
        while(iter != end){
            typename protocol_type::endpoint endpoint = *iter++;
            std::cout << endpoint << std::endl;
        }
        return true;
    }
    typedef SocketFlushBehaviours FlushBehaviours;
    gint64 bytesToWrite()const{ return numBytes; }

    void blockingRead()
    {
        while(m_sock_type.available() && !closing && !finished){
            ReadyRead();
        }
    }
protected:
    /**
     * @brief Socket::setClosing : the socket ought to be closed
     * @param c bool
     */
    void setClosing(bool c)
    {
        closing = c;
    }
    /**
     * @brief Socket::addBytes : helper to monitor the write queue length, by incrementation
     * @param num
     */
    void addBytes(gint64 num)
    {
        numBytes += num;
    }
public:
    /**
     * @brief Socket::setFinished : the protocol has been completed
     * @param f bool
     */
    void setFinished(bool f)
    {
        finished = f;
    }

    /**
     * @brief Socket::BytesWritten : closes the socket if write queue extremum has been reached [slot]
     * @param num qint64
     */
    void BytesWritten(gint64 num)
    {
        if(finished && (numBytes = std::max((gint64)0,numBytes-num)) <= 0){
            closing = true;
        }
    }

    /**
     * @brief Socket::ReadyRead : respond to bytes on the channel [slot]
     */
    void ReadyRead()
    {
        if(finished){
            return;
        }
        {
            READLOCK;
            gint64 nread = 1;
            do{
                boost::array<char, 1024> recv_buf;
                nread = m_sock_type.receive(asio::buffer(recv_buf));
                readBuffers.push_back(drumlin::buffers_type::value_type(new drumlin::Buffer(recv_buf.data(),nread)));
            }while(nread > 0);
        }
        if(!handler)
            return;
        bool replying = false;
//        if(socketType() == SocketType::TcpSocket) {
            if(handler->readyProcess(this)){
                Debug() << this << "::processTransmission";
                replying = handler->processTransmission(this);
            }
//        }else{
//            Debug() << this << "::receivePacket";
//            replying = handler->receivePacket(this);
//        }
        if(replying){
            Debug() << this << "::reply";
            replying = false;
            setFinished(handler->reply(this));
        }
    }

    /**
     * @brief Socket::Disconnect : respond to disconnection [slot]
     */
    void Disconnect()
    {
        if(handler)
            handler->disconnected(this);
    }

    /**
     * @brief Socket::peekData : look into the read queue, maybe cause processing (see SocketHandler::sort)
     * @param flushBehaviours quint8
     * @return byte_array
     */
    byte_array peekData(guint8 flushBehaviours)
    {
        READLOCK;
        byte_array allRead;
        if(handler && flushBehaviours & SocketFlushBehaviours::Sort){
            handler->sort(this,readBuffers);
        }
        if(flushBehaviours & SocketFlushBehaviours::Coalesce){
            for_each(readBuffers.begin(),readBuffers.end(),[&allRead](drumlin::buffers_type::value_type &buf){
                allRead.append(buf->data<void>(),buf->length());
            });
            if(flushBehaviours & SocketFlushBehaviours::Flush){
                readBuffers.clear();
            }
        }else{
            drumlin::buffers_type::value_type &buf(readBuffers.front());
            allRead.append(buf->data<void>(),buf->length());
        }
        if(flushBehaviours & SocketFlushBehaviours::Flush){
            readBuffers.clear();
        }
        return allRead;
    }

    /**
     * @brief Socket::flushWriteBuffer : send all the data to the comms channel
     * @return qint64
     */
    gint64 flushWriteBuffer()
    {
        WRITELOCK;
        Debug() << this << "  flushWriteBuffer";
        gint64 written(0);
        writeBuffers.erase(remove_if(writeBuffers.begin(),writeBuffers.end(),[&written,this](const drumlin::buffers_type::value_type &buf){
            addBytes(buf->length());
            gint64 result;
            try{
                system::error_code error;
                result = m_sock_type.send(boost::asio::buffer(buf->data<char>(),buf->length()),0,error);
                if(error == asio::error::eof){
                    setFinished(true);
                    return true;
                }else if(error && error != asio::error::message_size){
                    Critical() << error.message() << endl;
                    return true;
                }
            }catch(...){
                result = 0;
            }
            written += result;
            return true;
        }),writeBuffers.end());
        Debug() << this << "wrote" << written << "bytes";
        return written;
    }

    /**
     * @brief Socket::complete : flush and disconnectSlots
     * @return qint64
     */
    gint64 complete()
    {
        setFinished(true);
        gint64 written(flushWriteBuffer());
        if(handler)
            handler->completing(this,written);
        return written;
    }

    /**
     * @brief Socket::write : buffer some data to send
     * template<T> (const T &t)
     * @param string T
     * @return qint64
     */
    template <class T,typename boost::disable_if<typename boost::is_pointer<T>::type,int>::type = 0>
    gint64 write(T const& t,bool prepend = false)
    {
        WRITELOCK;
        numBytes += t.length();
        if(prepend)
            writeBuffers.push_front(drumlin::buffers_type::value_type(new drumlin::Buffer(t)));
        else
            writeBuffers.push_back(drumlin::buffers_type::value_type(new drumlin::Buffer(t)));
        return numBytes;
    }
    /**
     * @brief Socket::write : buffer some data to send
     * template<T> (const T &t)
     * @param string T
     * @return qint64
     */
    template <class T,typename boost::enable_if<typename boost::is_pointer<T>::type,int>::type = 0>
    gint64 write(T const t,bool prepend = false)
    {
        WRITELOCK;
        numBytes += t->length();
        if(prepend)
            writeBuffers.push_front(drumlin::buffers_type::value_type(new drumlin::Buffer(*t)));
        else
            writeBuffers.push_back(drumlin::buffers_type::value_type(new drumlin::Buffer(*t)));
        return numBytes;
    }

    /**
     * @brief Socket::write : buffer some const char* data
     * @param cstr const char*
     * @return quint64
     */

//    template<class Protocol>
//    gint64 write<IBuffer*>(IBuffer *buffer,bool prepend);

//    template<class Protocol>
//    gint64 write<byte_array>(byte_array const& bytes, bool prepend);

    void getStatus(json::value &status);

    socket_type &socket(){return m_sock_type;}

    friend class SocketHandler<Protocol>;
private:
    recursive_mutex write_buffer_mutex;
    recursive_mutex read_buffer_mutex;
    drumlin::buffers_type writeBuffers;
    drumlin::buffers_type readBuffers;
    bool finished = false;
    bool closing = false;
    gint64 numBytes = 0;
    SocketHandler<Protocol> *handler;
    void *tag;
    asio::io_service &m_io_service;
    socket_type m_sock_type;
};

} // namespace drumlin

#endif // SOCKET_H
