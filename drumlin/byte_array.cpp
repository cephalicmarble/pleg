#include "byte_array.h"

#include <string>
#include <sstream>
using namespace std;

namespace drumlin {

byte_array::byte_array()
{
    m_data = nullptr;
    m_length = 0;
}

byte_array::byte_array(byte_array &rhs)
{
    operator=(rhs);
}

byte_array::byte_array(byte_array &&rhs)
{
    operator=(rhs);
}

byte_array::byte_array(const void *mem,size_t length,bool takeOwnership)
{
    m_data = const_cast<void*>(mem);
    m_length = length;
    m_destroy = takeOwnership;
}

byte_array::byte_array(const std::string &owner)
{
    m_data = const_cast<void*>(static_cast<const void*>(owner.c_str()));
    m_length = owner.length();
}

byte_array::~byte_array()
{
    clear();
}

void byte_array::clear()
{
    if(m_destroy){
        free(m_data);
        m_destroy = false;
    }
    m_data = nullptr;
    m_length = 0;
}

byte_array &byte_array::operator=(const byte_array &rhs)
{
    m_destroy = rhs.m_destroy;
    const_cast<byte_array&>(rhs).m_destroy = false;
    m_data = rhs.m_data;
    m_length = rhs.m_length;
    return *this;
}

byte_array &byte_array::operator=(const char *pc)
{
    clear();
    m_data = const_cast<void*>(static_cast<const void*>(pc));
    m_length = strlen(pc);
    return *this;
}

byte_array byte_array::fromRawData(char *pc,size_t start,size_t length)
{
    byte_array bytes;
    bytes.append(pc+start,length!=string::npos?length:strlen(pc+start));
    return bytes;
}

byte_array byte_array::fromRawData(void *mem,size_t length)
{
    byte_array bytes;
    bytes.append(mem,length);
    return bytes;
}

byte_array byte_array::fromRawData(std::string str)
{
    byte_array bytes;
    bytes.append(str.c_str(),str.length());
    return bytes;
}

byte_array byte_array::readAll(istream &strm)
{
    stringstream ss;
    char pc[1024];
    streamsize sz = sizeof(pc);
    while(sizeof(pc) == (sz = strm.readsome(pc,sizeof(pc)))){
        ss << std::string(pc,sz);
    }
    byte_array bytes;
    bytes.append(ss.str().c_str(),ss.str().length());
    return bytes;
}

void byte_array::append(std::string const& str)
{
    append(str.c_str(),str.length());
}

void byte_array::append(std::string & str)
{
    append(str.c_str(),str.length());
}

void byte_array::append(const void *m_next,size_t length)
{
    if(!length)
        return;
    char *pdest;
    if(m_data){
        pdest = (char*)realloc(m_data,m_length+length);
    }else{
        pdest = (char*)malloc(m_length+length);
    }
    memmove(pdest+m_length,m_next,length);
    if(m_data != nullptr)free(m_data);
    m_data = pdest;
    m_length += length;
    m_destroy = true;
}

/**
     * @brief Buffer::Buffer : copy from data
     * @param _data void*
     * @param _len qint64
     */
Buffer::Buffer(void*_data,gint64 _len):type(FreeBuffer)
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
Buffer::Buffer(string const& _str):type(FreeBuffer)
{
    const char *str(_str.c_str());
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
Buffer::Buffer(const IBuffer *buffer):type(CacheBuffer)
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
        return byte_array::fromRawData(const_cast<void*>(data<void>()),length());
    case CacheBuffer:
        return byte_array::fromRawData(const_cast<void*>(buffers._buffer->data()),buffers._buffer->length());
    default:
        return byte_array();
    }
}

ostream& operator<< (ostream &strm, const drumlin::byte_array &bytes)
{
    strm.write((char*)bytes.data(),bytes.length());
    return strm;
}

istream& operator>> (istream& strm, drumlin::byte_array &bytes)
{
    stringstream ss;
    ss << strm.rdbuf();
    bytes.append(ss.str().c_str(),ss.str().length());
    return strm;
}

} //namespace drumlin
