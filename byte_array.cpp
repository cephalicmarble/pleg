#include "byte_array.h"

#include <sstream>

namespace drumlin {

byte_array::byte_array()
{
    m_data = nullptr;
    m_length = 0;
}

byte_array::byte_array(void *mem,size_t length)
{
    m_data = mem;
    m_length = length;
}

static byte_array byte_array::fromRawData(void *mem,size_t length)
{
    byte_array bytes;
    bytes.append(mem,length);
    return bytes;
}

static byte_array byte_array::readAll(istream &strm)
{
    stringstream ss;
    char pc[1024];
    streamsize sz = sizeof(pc);
    while(sizeof(pc) == (sz = strm.readsome(pc,sizeof(pc)))){
        ss << std::string(pc,sz);
    }
    byte_array bytes;
    bytes.append(ss.data(),ss.length());
    return bytes;
}

void byte_array::append(std::string &str)
{
    append(str.c_str(),str.length());
}

void byte_array::append(void *m_next,size_t length)
{
    void *pdest = malloc(m_length+length);
    memmove(pdest,data(),m_length);
    memmove(pdest+m_length,m_next,length);
    if(m_data != nullptr)free(m_data);
    m_data = pdest;
    m_length += length;
}

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
Buffer::Buffer(string const& str):type(FreeBuffer)
{
    const char *str(str.c_str());
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
gint64 Buffer::length()
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

} //namespace drumlin
