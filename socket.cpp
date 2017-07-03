#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include <algorithm>
using namespace std;
#include <boost/thread/recursive_mutex.hpp>
using namespace boost;
#include "socket.h"
#include "exception.h"
#include "thread.h"

using namespace drumlin;

recursive_mutex Socket::mutex;

void Socket::getStatus(json::value *status)
{
    SOCKETLOCK
    if(state()!=Socket::ConnectedState){
        return;
    }
    json::object_t &obj(status->get_object());
    obj.insert({"available",bytesAvailable()});
    obj.insert({"buffered",bytesToWrite()});
}

/**
 * @brief Socket::BytesWritten : closes the socket if write queue extremum has been reached [slot]
 * @param num qint64
 */
void Socket::BytesWritten(qint64 num)
{
    SOCKETLOCK
    if(finished && (numBytes = qMax((qint64)0,numBytes-num)) <= 0){
        close();
    }
}

/**
 * @brief Socket::ReadyRead : respond to bytes on the channel [slot]
 */
void Socket::ReadyRead()
{
    SOCKETLOCK
    if(finished){
        return;
    }
    const qint64 bufsize = 1024;
    char buf[bufsize];
    {
        QMutexLocker ml2(&read_buffer_mutex);
        qint64 nread = 1;
        do{
            nread = read(buf,bufsize);
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
void Socket::Disconnect()
{
    SOCKETLOCK
    QMutexLocker ml2(&read_buffer_mutex);
    if(handler)
        handler->disconnectedImpl(this);
}

/**
 * @brief Socket::peekData : look into the read queue, maybe cause processing (see SocketHandler::sort)
 * @param flushBehaviours quint8
 * @return byte_array
 */
byte_array Socket::peekData(quint8 flushBehaviours)
{
    SOCKETLOCK
    QMutexLocker ml2(&read_buffer_mutex);
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
qint64 Socket::flushWriteBuffer()
{
    SOCKETLOCK
    qint64 written(0);
    qDebug() << this << "  flushWriteBuffer";
    writeBuffers.erase(remove_if(writeBuffers.begin(),writeBuffers.end(),[&written,this](const Socket::buffers_type::value_type &buf){
        addBytes(buf->length());
        qint64 result;
        try{
            result = writeData(buf->data<char>(),buf->length());
            if(result<0){
                qDebug() << "writeData returned " << result;
            }
        }catch(...){
            result = 0;
        }
        written += result;
        return true;
    }),writeBuffers.end());
    qDebug() << this << "wrote" << written << "bytes";
    return written;
}

namespace Socket {

    /**
     * @brief Buffer::Buffer : copy from data
     * @param _data void*
     * @param _len qint64
     */
    Buffer::Buffer(void*_data,qint64 _len):type(FreeBuffer)
    {
        buffers.free_buffer.len = _len;
        buffers.free_buffer.data = (char*)malloc(buffers.free_buffer.len);
        memcpy(buffers.free_buffer.data,_data,buffers.free_buffer.len);
    }

    /**
     * @brief Buffer::Buffer : copy from byte_array
     * @param bytes byte_array
     */
    Buffer::Buffer(byte_array const& bytes):type(FreeBuffer)
    {
        buffers.free_buffer.len = bytes.length();
        buffers.free_buffer.data = (char*)malloc(buffers.free_buffer.len);
        memcpy(buffers.free_buffer.data,(void*)bytes.data(),buffers.free_buffer.len);
    }

    /**
     * @brief Buffer::Buffer : copy from tring
     * @param  tring
     */
    Buffer::Buffer(tring const& ):type(FreeBuffer)
    {
        const char *str(.toStdString().c_str());
        buffers.free_buffer.len = strlen(str);
        buffers.free_buffer.data = strdup(str);
    }

    /**
     * @brief Buffer::Buffer : copy from cstr
     * @param cstr const char*
     */
    Buffer::Buffer(const char *cstr):type(FreeBuffer)
    {
        buffers.free_buffer.len = strlen(cstr);
        buffers.free_buffer.data = strdup(cstr);
    }

    /**
     * @brief Buffer::Buffer : reference a buffer
     * @param buffer Buffers::Buffer*
     */
    Buffer::Buffer(const Buffers::Buffer *buffer):type(CacheBuffer)
    {
        buffers._buffer = buffer;
    }

    /**
     * @brief Buffer::~Buffer : free any copied data
     */
    Buffer::~Buffer()
    {
        if(type == FreeBuffer){
            free(buffers.free_buffer.data);
        }
    }
    /**
     * @brief Buffer::length
     * @return qint64
     */
    qint64 Buffer::length()
    {
        switch(type){
        case FreeBuffer:
            return buffers.free_buffer.len;
        case CacheBuffer:
            return buffers._buffer->length();
        }
        return 0;
    }
    /**
     * @brief Buffer::operator byte_array
     */
    Buffer::operator byte_array()
    {
        switch(type){
        case FreeBuffer:
            return byte_array::fromRawData(data<char>(),length());
        case CacheBuffer:
            return *buffers._buffer;
        }
        return byte_array();
    }
}

/**
 * @brief Socket::complete : flush and disconnectSlots
 * @return qint64
 */
qint64 Socket::complete()
{
    SOCKETLOCK
    setFinished(true);
    qint64 written(flushWriteBuffer());
    if(handler)
        handler->completingImpl(this,written);
    return written;
}

/**
 * @brief Socket::write : buffer some const char* data
 * @param cstr const char*
 * @return quint64
 */
qint64 Socket::write(const char *cstr,bool prepend)
{
    SOCKETLOCK
    numBytes += strlen(cstr);
    if(prepend)
        writeBuffers.push_front(Socket::buffers_type::value_type(new Socket::Buffer(cstr)));
    else
        writeBuffers.push_back(Socket::buffers_type::value_type(new Socket::Buffer(cstr)));
    return numBytes;
}

qint64 Socket::write(Buffers::Buffer *buffer,bool prepend)
{
    SOCKETLOCK
    numBytes += buffer->length();
    if(prepend)
        writeBuffers.push_front(Socket::buffers_type::value_type(new Socket::Buffer(buffer)));
    else
        writeBuffers.push_back(Socket::buffers_type::value_type(new Socket::Buffer(buffer)));
    return numBytes;
}
