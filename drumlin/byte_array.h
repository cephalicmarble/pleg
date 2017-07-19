#ifndef BYTE_ARRAY_H
#define BYTE_ARRAY_H

#include <string>
#include <iostream>
#include <fstream>
#include <istream>
#include <ostream>
#include <memory>
using namespace std;
#include <boost/array.hpp>
using namespace boost;
#include "object.h"
#include "glib.h"
#include "logger.h"

namespace drumlin {

class byte_array
{
public:
    typedef gint64 size_type;
    byte_array();
    byte_array(const void *mem,size_t length,bool takeOwnership = false);
    byte_array(const string &owner);
    byte_array(byte_array &rhs);
    byte_array(byte_array &&rhs);
    ~byte_array();
    void clear();
    byte_array &operator=(const char *pc);
    byte_array &operator=(const byte_array &pc);
    static byte_array fromRawData(char *pc,size_t start,size_t length);
    static byte_array fromRawData(void *mem,size_t length);
    static byte_array fromRawData(string str);
    static byte_array readAll(istream &strm);
    char *cdata()const{ return (char*)m_data; }
    void *data()const{ return m_data; }
    size_type  length()const{ return m_length; }
    void truncate(size_type len){ if(m_data && m_length > std::max(m_length,len)){((char*)m_data)[m_length=len] = 0;} }
    std::string string(){ return m_length?std::string((char*)m_data,0,m_length):std::string(); }
    void *release(){ m_destroy = false; return data(); }
    void append(std::string &str);
    void append(const void *m_next,size_t length);
    void append(std::string const& str);
private:
    void *m_data;
    size_type m_length;
    bool m_destroy = false;
    friend ostream& operator<< (ostream& strm, const drumlin::byte_array &bytes);
    friend istream& operator>> (istream& strm, drumlin::byte_array &bytes);
};

class IBuffer {
public:
    virtual const void *data()const=0;
    virtual guint32 length()const=0;
};

/*
 * Buffer union class to allow single buffering.
 */
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
        const IBuffer *_buffer;
        free_buffer_t free_buffer;
        buffers_t(){}
        ~buffers_t(){}
    }buffers;
public:
    Buffer(ptr_type _data,gint64 _len);
    Buffer(byte_array const& bytes);
    Buffer(string const& );
    Buffer(const char *cstr);
    Buffer(const IBuffer *buffer);
    operator byte_array();
    ~Buffer();
    template <class T>
    const T*data()
    {
        switch(type){
        case FreeBuffer:
            return (const T*)buffers.free_buffer.data;
        case CacheBuffer:
            return (const T*)buffers._buffer->data();
        }
        return nullptr;
    }
    gint64 length();
};
/**
 * @brief buffers_type : the type of the socket's internal buffer vector
 */
typedef std::list<std::unique_ptr<Buffer>> buffers_type;

extern logger& operator<< (logger& strm, const drumlin::byte_array &bytes);
extern istream& operator>> (istream& strm, drumlin::byte_array &bytes);

} // namespace drumlin

#endif // BYTE_ARRAY_H
