#ifndef SOCKET_H
#define SOCKET_H

#include "drumlin.h"
#include "tao_forward.h"
using namespace tao;
#include <memory>
#include <string>
#include <list>
#include <mutex>
using namespace std;
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
class readHandler;
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
    virtual void completing(Socket *socket)=0;
    virtual void sort(Socket *socket,drumlin::buffers_type &buffers)=0;
    virtual void disconnected(Socket *socket)=0;
    virtual void error(Socket *socket,boost::system::error_code &ec)=0;
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
    typedef SocketAdapter<Protocol> Self;
    typedef typename Socket<Protocol>::recv_buf_type recv_buf_type;

    SocketAdapter(Socket<Protocol> *);
    SocketAdapter(Self &self):m_socket(self.m_socket){}

    void error(boost::system::error_code ec);
private:
    socket_type *m_socket;
};

#define READLOCK  std::lock_guard<std::recursive_mutex> l1(const_cast<std::recursive_mutex&>(read_buffer_mutex));
#define WRITELOCK std::lock_guard<std::recursive_mutex> l2(const_cast<std::recursive_mutex&>(write_buffer_mutex));

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
    typedef SocketAdapter<Protocol> adapter_type;
    typedef boost::array<char, 1024> recv_buf_type;
//    Socket(boost::asio::io_service &io_service,Object *parent = 0,Handler *_handler = 0)
//        :Object(parent),handler(_handler),m_io_service(io_service),m_sock_type(new socket_type(io_service))
//    {
//        READLOCK
//        WRITELOCK
//        readBuffers.clear();
//        writeBuffers.clear();
//    }
    Socket(asio::io_service &io_service, Object *parent, Handler *_handler,socket_type *socket)
        :Object(parent),handler(_handler),m_io_service(io_service),m_sock_type(socket)
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
        m_connection = nullptr;
        handler = nullptr;
        m_sock_type->close();
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
    Connection<Protocol> *getConnection(){ return m_connection; }
    typedef SocketFlushBehaviours FlushBehaviours;
    gint64 bytesToWrite()const{ return numBytes; }

    /**
     * @brief Socket::setClosing : the socket ought to be closed
     * @param c bool
     */
    void setClosing(bool c)
    {
        closing = c;
    }
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
        if(closing)
            return;
        size_t sz;
        if(synchronousRead){
            try{
                sz = socket().receive(asio::buffer(m_recv_buf.data(),m_recv_buf.max_size()));
                bytesRead(boost::system::error_code(),sz);
            }catch(boost::system::system_error error){
                bytesRead(error.code(),sz);
            }
        }else{
            socket().async_receive(asio::buffer(m_recv_buf.data(),m_recv_buf.max_size()),
                               boost::bind(&Self::bytesRead,this,boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
        }
    }

    void bytesRead(boost::system::error_code ec,size_t sz)
    {
        READLOCK;
        if(ec){
            adapter_type(this).error(ec);
            return;
        }
        readBuffers.push_back(drumlin::buffers_type::value_type(new drumlin::Buffer(m_recv_buf.data(),sz)));
        m_bytes_transferred += sz;
        process();
        if(!finished){
            reading();
        }
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
            if(finished)
                handler->completing(this);
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
        char *freud(nullptr);
        if(handler && flushBehaviours & SocketFlushBehaviours::Sort){
            handler->sort(this,readBuffers);
        }
        if(freud){
            free(freud);
        }
        size_t length(0);
        if(flushBehaviours & SocketFlushBehaviours::Coalesce){
            for(auto &buf : readBuffers){
                length += buf->length();
            }
            if(length){
                freud = (char*)malloc(length+1);
                if(!freud){
                    Critical() << __func__ << "ENOMEM";
                    return byte_array("");
                }
                char *pos(freud);
                for(auto &buf : readBuffers){
                    memmove(pos,buf->data<void>(),buf->length());
                    pos += buf->length();
                }
            }else{
                freud = nullptr;
            }
        }else{
            drumlin::buffers_type::value_type &buf(readBuffers.front());
            freud = (char*)malloc(1+(length = buf->length()));
            memmove(freud,buf->data<void>(),length);
        }
        if(flushBehaviours & SocketFlushBehaviours::Flush){
            readBuffers.clear();
        }
        if(freud)
            freud[length] = 0;
        return byte_array(freud,length,true);
    }

    void writing()
    {
        if(closing)
            return;
        WRITELOCK;
        auto p_buffer = writeBuffers.front().get();
        size_t sz;
        if(synchronousWrite){
            try{
                sz = socket().send(asio::buffer(p_buffer->data<void>(),p_buffer->length()));
                bytesWritten(boost::system::error_code(),sz);
            }catch(boost::system::system_error error){
                bytesWritten(error.code(),sz);
            }
        }else{
            socket().async_send(asio::buffer(p_buffer->data<void>(),p_buffer->length()),
                            boost::bind(&Self::bytesWritten,this,boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred));
        }
    }

    void bytesWritten(boost::system::error_code ec, std::size_t sz)
    {
        WRITELOCK;
        if(ec){
            adapter_type(this).error(ec);
            return;
        }
        size_t d(distance(writeBuffers.begin(),writeBuffers.end()));
        if(d-->0){
            if(sz < (size_t)writeBuffers.front()->length()){
                m_bytes_written += sz;
                return;
            }
            writeBuffers.pop_front();
        }
        if(d){
            if(closing)
                return;
            writing();
        }else if(finished){
            handler->completing(this);
        }
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

    socket_type &socket(){return *m_sock_type;}

    friend class SocketHandler<Protocol>;
    friend class SocketAdapter<Protocol>;
    void synchronousReads(bool sync){synchronousRead = sync;}
    void synchronousWrites(bool sync){synchronousWrite = sync;}
private:
    bool synchronousRead = false;
    bool synchronousWrite = false;
    std::size_t m_bytes_transferred;
    recv_buf_type m_recv_buf;
    std::size_t m_bytes_written;
    std::recursive_mutex write_buffer_mutex;
    std::recursive_mutex read_buffer_mutex;
    drumlin::buffers_type writeBuffers;
    drumlin::buffers_type readBuffers;
    bool finished = false;
    bool closing = false;
    gint64 numBytes = 0;
    SocketHandler<Protocol> *handler = nullptr;
    Connection<Protocol> *m_connection = nullptr;
    void *tag;
    asio::io_service &m_io_service;
    socket_type *m_sock_type;
};

} // namespace drumlin

#endif // SOCKET_H
