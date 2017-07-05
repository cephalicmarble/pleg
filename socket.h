#ifndef SOCKET_H
#define SOCKET_H

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
#include "buffer.h"
#include "glib.h"

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

template <class SockType>
class Socket;

/**
 * @brief The SocketHandler class : abstract class to generalize over sockets
 */
template <class SockType>
class SocketHandler {
protected:
    typedef drumlin::Socket<SockType> Socket;
    virtual bool processTransmission(Socket *socket)=0;
    virtual bool receivePacket(Socket *socket)=0;
    virtual bool readyProcess(Socket *socket)=0;
    virtual void reply(Socket *socket)=0;
    virtual void completing(Socket *socket, qint64 written)=0;
    virtual void sort(Socket *socket,Socket::buffers_type &buffers)=0;
    virtual void disconnected(Socket *socket)=0;
};

#define READLOCK  lock_guard l1(&read_buffer_mutex);
#define WRITELOCK lock_guard l2(&write_buffer_mutex);

/**
 * @brief The Socket class
 */
template <class SockType= asio::ip::tcp::socket>
class Socket
{
public:
    typedef SocketHandler<SockType> Handler;
    Socket(SockType &sock_type,Object *parent = 0,Handler *handler = 0):m_sock_type(sock_type),handler(_handler)
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
        m_sock_type.disconnect();
        writeBuffers.erase(std::remove_if(writeBuffers.begin(),writeBuffers.end(),[](auto &a){return true;}),writeBuffers.end());
        readBuffers.erase(std::remove_if(readBuffers.begin(),readBuffers.end(),[](auto &a){return true;}),readBuffers.end());
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
    typedef SocketFlushBehaviours FlushBehaviours;
    bool disconnectSlots();
    gint64 bytesToWrite()const{ return numBytes; }

    void getStatus(json::value *status)
    {
        json::object_t &obj(status->get_object());
        obj.insert({"available",bytesAvailable()});
        obj.insert({"buffered",bytesToWrite()});
    }

    void blockingRead()
    {
        while(m_sock_type.available() && !closing && !finished){
            ReadyRead();
        }
    }
protected:
    bool connectSlots();
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
    virtual bool setSocketDescriptor(qintptr socketDescriptor, SocketState socketState = ConnectingState, OpenMode openMode = ReadWrite);
    virtual void connectToHost(const tring &hostName, guint16 port, OpenMode openMode = ReadWrite, NetworkLayerProtocol protocol = AnyIPProtocol);
    byte_array peekData(guint8 flushBehaviours);
    gint64 write(const char *cstr,bool prepend = false);
    gint64 write(Buffers::Buffer *buffer,bool prepend = false);
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
            writeBuffers.push_front(Socket::buffers_type::value_type(new Socket::Buffer(t)));
        else
            writeBuffers.push_back(Socket::buffers_type::value_type(new Socket::Buffer(t)));
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
            writeBuffers.push_front(Socket::buffers_type::value_type(new Socket::Buffer(*t)));
        else
            writeBuffers.push_back(Socket::buffers_type::value_type(new Socket::Buffer(*t)));
        return numBytes;
    }

    /**
     * @brief Socket::BytesWritten : closes the socket if write queue extremum has been reached [slot]
     * @param num qint64
     */
    void BytesWritten(gint64 num)
    {
        if(finished && (numBytes = qMax((gint64)0,numBytes-num)) <= 0){
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
        const qint64 bufsize = 1024;
        char buf[bufsize];
        {
            READLOCK;
            qint64 nread = 1;
            do{
                nread = m_sock_type.read(buf,bufsize);
                if(0<nread)readBuffers.push_back(Socket::buffers_type::value_type(new Socket::Buffer(buf,nread)));
            }while(nread > 0);
        }
        if(!handler)
            return;
        bool replying = false;
        if(socketType() == SocketType::TcpSocket) {
            if(handler->readyProcessImpl(this)){
                qDebug() << this << "::processTransmission";
                replying = handler->processTransmissionImpl(this);
            }
        }else{
            qDebug() << this << "::receivePacket";
            replying = handler->receivePacketImpl(this);
        }
        if(replying){
            qDebug() << this << "::reply";
            replying = false;
            setFinished(handler->replyImpl(this));
        }
    }

    /**
     * @brief Socket::Disconnect : respond to disconnection [slot]
     */
    void Disconnect()
    {
        if(handler)
            handler->disconnectedImpl(this);
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
            handler->sortImpl(this,readBuffers);
        }
        if(flushBehaviours & SocketFlushBehaviours::Coalesce){
            for_each(readBuffers.begin(),readBuffers.end(),[&allRead](Socket::buffers_type::value_type &buf){
                allRead.append(*buf);
            });
            if(flushBehaviours & SocketFlushBehaviours::Flush){
                readBuffers.clear();
            }
        }else{
            allRead.append(*readBuffers.front());
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
        writeBuffers.erase(remove_if(writeBuffers.begin(),writeBuffers.end(),[&written,this](const Socket::buffers_type::value_type &buf){
            addBytes(buf->length());
            gint64 result;
            try{
                result = writeData(buf->data<char>(),buf->length());
                if(result<0){
                    Debug() << "writeData returned " << result;
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
    gint64 Socket::complete()
    {
        setFinished(true);
        gint64 written(flushWriteBuffer());
        if(handler)
            handler->completingImpl(this,written);
        return written;
    }

    /**
     * @brief Socket::write : buffer some const char* data
     * @param cstr const char*
     * @return quint64
     */
    gint64 Socket::write(const char *cstr,bool prepend)
    {
        WRITELOCK;
        numBytes += strlen(cstr);
        if(prepend)
            writeBuffers.push_front(Socket::buffers_type::value_type(new Socket::Buffer(cstr)));
        else
            writeBuffers.push_back(Socket::buffers_type::value_type(new Socket::Buffer(cstr)));
        return numBytes;
    }

    gint64 Socket::write(Buffers::Buffer *buffer,bool prepend)
    {
        WRITELOCK;
        numBytes += buffer->length();
        if(prepend)
            writeBuffers.push_front(Socket::buffers_type::value_type(new Socket::Buffer(buffer)));
        else
            writeBuffers.push_back(Socket::buffers_type::value_type(new Socket::Buffer(buffer)));
        return numBytes;
    }

    gint64 Socket::write(byte_array const& bytes, bool prepend)
    {
        WRITELOCK;
        numBytes += bytes.length();
        if(prepend)
            writeBuffers.push_front(Socket::buffers_type::value_type(new Socket::Buffer(bytes)));
        else
            writeBuffers.push_back(Socket::buffers_type::value_type(new Socket::Buffer(bytes)));
        return numBytes;
    }

    friend class SocketHandler;
private:
    recursive_mutex write_buffer_mutex;
    recursive_mutex read_buffer_mutex;
    Socket::buffers_type writeBuffers;
    Socket::buffers_type readBuffers;
    bool finished = false;
    bool closing = false;
    gint64 numBytes = 0;
    SocketHandler<SockType> *handler;
    void *tag;
    SockType &m_sock_type;
};

#endif // SOCKET_H
