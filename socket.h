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

#define socketTasks (socketRead,socketWrite)
ENUM(socketTask,socketTasks)

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

template <class Protocol>
class Connection;

template <class Protocol>
class writeHandler;

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
    virtual void completing(Socket *socket, writeHandler<Socket> *)=0;
    virtual void sort(Socket *socket,drumlin::buffers_type &buffers)=0;
    virtual void disconnected(Socket *socket)=0;
};

struct ISocketTask
{
    virtual ~ISocketTask(){}
    virtual socketTask type()=0;
    void setTaskFinished(bool flag)
    {
        m_finished = flag;
    }
private:
    bool m_finished = false;
};

template <class Protocol>
struct SocketAdapter
{
    typedef Socket<Protocol> socket_type;
    SocketAdapter(Socket<Protocol> *);
    drumlin::buffers_type &getWriteBuffers();
    drumlin::buffers_type &getReadBuffers();
    bool available();
    void process();
    void async_receive(ISocketTask *);
    void async_send(ISocketTask *);
    void error(boost::system::error_code ec);
    void completing(writeHandler<Socket<Protocol>> *);
    size_t writeQueueLength();
private:
    socket_type *m_socket;
};

template <class Socket,int N>
class readHandler : public ISocketTask
{
public:
    typedef typename Socket::socket_type socket_type;
    readHandler(Socket *that):m_that(that)
    {
        kick();
    }
    void operator()(boost::system::error_code ec, std::size_t sz)
    {
        if(ec){
            m_that.error(ec);
            return;
        }
        m_sz = sz;
        m_that.getReadBuffers().push_back(drumlin::buffers_type::value_type(new drumlin::Buffer(recv_buf.data(),sz)));
        if(m_that.available())
            kick();
        else{
            setTaskFinished(true);
            m_that.process();
        }
    }
    void kick(){
        m_that.async_receive(this);
    }
    socketTask type(){ return socketRead; }
    friend class SocketAdapter<typename Socket::protocol_type>;
private:
    SocketAdapter<typename Socket::protocol_type> m_that;
    std::size_t m_sz;
    boost::array<char, N> recv_buf;
};

template <class Socket>
class writeHandler : public ISocketTask
{
public:
    typedef typename Socket::socket_type socket_type;
    writeHandler(Socket *that):m_that(that)
    {
        kick();
    }
    void operator()(boost::system::error_code ec, std::size_t sz)
    {
        if(ec){
            m_that.error(ec);
            return;
        }
        drumlin::buffers_type::value_type &buffer(m_that.getWriteBuffers().front());
        if(sz < (size_t)buffer->length()){
            bytesWritten += sz;
            m_sz += sz;
            return;
        }
        m_that.getWriteBuffers().pop_front();
        numWritten++;
        if(m_that.writeQueueLength()){
            kick();
        }else{
            setTaskFinished(true);
            m_that.completing(this);
        }
    }
    void kick(){
        p_buffer = m_that.getWriteBuffers().front().get();
        m_sz = 0;
        m_that.async_send(this);
    }
    socketTask type(){ return socketWrite; }
    friend class SocketAdapter<typename Socket::protocol_type>;
private:
    SocketAdapter<typename Socket::protocol_type> m_that;
    std::size_t m_sz;
    gint64 numWritten;
    gint64 bytesWritten;
    typename drumlin::buffers_type::value_type::element_type *p_buffer;
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
    typedef Protocol protocol_type;
    typedef Socket<Protocol> Self;
    typedef list<std::unique_ptr<ISocketTask>> task_list_type;
    typedef readHandler<Self,1024> readHandler_type;
    typedef writeHandler<Self> writeHandler_type;
    typedef SocketAdapter<Protocol> adapter_type;
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
        m_tasks.clear();
        writeBuffers.erase(std::remove_if(writeBuffers.begin(),writeBuffers.end(),[](auto &){return true;}),writeBuffers.end());
        readBuffers.erase(std::remove_if(readBuffers.begin(),readBuffers.end(),[](auto &){return true;}),readBuffers.end());
    }
    /**
     * @brief setTag : associate a void* with the socket
     * @param _tag void*
     */
    Self &setTag(void *_tag){ tag = (void*)_tag; return *this; }
    Self &setConnection(Connection<Protocol> *connection){ m_connection = connection; return *this; }
    Self &setHandler(Handler *_handler){ handler = _handler; return *this; }
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

    void reading()
    {
        m_tasks.push_back(task_list_type::value_type(new readHandler_type(this)));
    }

    void process()
    {
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

    void writing()
    {
        m_tasks.push_back(task_list_type::value_type(new writeHandler<Self>(this)));
    }

    size_t writeQueueLength()
    {
        WRITELOCK;
        return distance(writeBuffers.begin(),writeBuffers.end());
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

    void getStatus(json::value &status);

    socket_type &socket(){return m_sock_type;}

    friend class SocketHandler<Protocol>;
    friend class SocketAdapter<Protocol>;
private:
    recursive_mutex write_buffer_mutex;
    recursive_mutex read_buffer_mutex;
    drumlin::buffers_type writeBuffers;
    drumlin::buffers_type readBuffers;
    bool finished = false;
    bool closing = false;
    gint64 numBytes = 0;
    SocketHandler<Protocol> *handler = nullptr;
    Connection<Protocol> *m_connection = nullptr;
    void *tag;
    asio::io_service &m_io_service;
    socket_type m_sock_type;
    task_list_type m_tasks;
};

} // namespace drumlin

#endif // SOCKET_H
