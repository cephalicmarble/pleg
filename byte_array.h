#ifndef BYTE_ARRAY_H
#define BYTE_ARRAY_H

#include <string>
#include <iostream>
#include <fstream>
#include <istream>
#include <ostream>
using namespace std;
#include "object.h"
#include "glib.h"

namespace drumlin {

class byte_array : public Object
{
public:
    byte_array();
    byte_array(void *mem,size_t length);
    static byte_array fromRawData(void *mem,size_t length);
    static byte_array readAll(istream &strm);
    void *data()const{ return m_data; }
    size_t length()const{ return m_length; }
    std::string string()const{ return m_length?std::string((char*)m_data):std::string(); }
    operator string()const{ return string(); }
    void append(std::string &str);
    void append(void *m_next,size_t length);
private:
    void *m_data;
    size_t m_length;
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
        const Buffers::Buffer *_buffer;
        free_buffer_t free_buffer;
        buffers_t(){}
        ~buffers_t(){}
    }buffers;
public:
    Buffer(ptr_type _data,gint64 _len);
    Buffer(byte_array const& bytes);
    Buffer(string const& );
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

} // namespace drumlin

#endif // BYTE_ARRAY_H
