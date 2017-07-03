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

/*
 * Buffer union class to allow single buffering.
 */
namespace Socket {
    /**
     * @brief The BufferType enum :
     * CacheBuffer = 1  Buffer*
     * FreeBuffer = 2   void*
     */
    enum BufferType {
        CacheBuffer,
        FreeBuffer
    };
    /**
     * @brief The Buffer struct
     */
    struct Buffer {
        BufferType type;
        typedef void *ptr_type;
        struct free_buffer_t {
            char* data;
            gint64 len;
        };
        union buffers_t {
            const Buffers::Buffer *_buffer;
            free_buffer_t free_buffer;
            buffers_t(){}
            ~buffers_t(){}
        }buffers;
    public:
        Buffer(ptr_type _data,gint64 _len);
        Buffer(byte_array const& bytes);
        Buffer(tring const& );
        Buffer(const char *cstr);
        Buffer(const Buffers::Buffer *buffer);
        operator byte_array();
        ~Buffer();
        template <class T>
        const T*data()
        {
            switch(type){
            case FreeBuffer:
                return (const T*)buffers.free_buffer.data;
            case CacheBuffer:
                return buffers._buffer->data<T>();
            }
            return nullptr;
        }
        gint64 length();
    };
    /**
     * @brief buffers_type : the type of the socket's internal buffer vector
     */
    typedef std::list<std::unique_ptr<Buffer>> buffers_type;
}

/**
 * @brief The SocketHandler class : abstract class to generalize over sockets
 */
template <class SockType>
class SocketHandler {
protected:
    typedef drumlin::Socket<SockType> Socket;
    /*
     * These functions apply QMutexLocker to the socket mutex.
     */
    virtual bool processTransmission(Socket *socket)=0;
    virtual bool receivePacket(Socket *socket)=0;
    virtual bool readyProcess(Socket *socket)=0;
    virtual void reply(Socket *socket)=0;
    virtual void completing(Socket *socket, qint64 written)=0;
    virtual void sort(Socket *socket,Socket::buffers_type &buffers)=0;
    virtual void disconnected(Socket *socket)=0;
    virtual ~SocketHandler(){}
    /*
     * Necessary
     */
    friend class Socket;
};

/**
 * @brief The Socket class
 */
template <class Socket = asio::ip::tcp::socket>
class Socket
{
public:
    typedef SocketHandler<SockType> Handler;
    Socket(SockType &sock_type,Object *parent = 0,Handler *handler = 0):m_sock_type(sock_type),handler(_handler)
    {
        readBuffers.clear();
        writeBuffers.clear();
    }
    ~Socket()
    {
        lock_guard l1(&read_buffer_mutex);
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

    void getStatus(json::value *status);
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
        lock_guard l(&mutex);
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
        lock_guard l(&mutex);
        numBytes += t->length();
        if(prepend)
            writeBuffers.push_front(Socket::buffers_type::value_type(new Socket::Buffer(*t)));
        else
            writeBuffers.push_back(Socket::buffers_type::value_type(new Socket::Buffer(*t)));
        return numBytes;
    }

    gint64 flushWriteBuffer();
    gint64 complete();

    friend class SocketHandler;
private:
    static recursive_mutex mutex;
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
