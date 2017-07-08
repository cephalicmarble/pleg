#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <iostream>
using namespace std;
#include <boost/thread.hpp>
using namespace boost;

namespace drumlin {

class logger
{
public:
    logger(ostream &strm):stream(strm){}
    ~logger(){
        stream << endl;
    }
    logger(logger &rhs):stream(rhs.stream),once(rhs.once){}
    operator ostream&()
    {
        return stream;
    }
    logger &operator<<(const boost::thread::id &id)
    {
        if(!once)
            stream << " ";
        stream << id;
        once = false;
        return *this;
    }
    logger &operator<<(const string & str)
    {
        if(!once)
            stream << " ";
        stream << str;
        once = false;
        return *this;
    }
    logger &operator<<(const char *str)
    {
        if(!once)
            stream << " ";
        stream << str;
        once = false;
        return *this;
    }
    logger &operator<<(const unsigned long int i)
    {
        if(!once)
            stream << " ";
        stream << i;
        once = false;
        return *this;
    }
    logger &operator<<(const long int i)
    {
        if(!once)
            stream << " ";
        stream << i;
        once = false;
        return *this;
    }
    logger &operator<<(const unsigned int i)
    {
        if(!once)
            stream << " ";
        stream << i;
        once = false;
        return *this;
    }
    logger &operator<<(const int i)
    {
        if(!once)
            stream << " ";
        stream << i;
        once = false;
        return *this;
    }
    logger &operator<<(const unsigned short int i)
    {
        if(!once)
            stream << " ";
        stream << i;
        once = false;
        return *this;
    }
    logger &operator<<(const short int i)
    {
        if(!once)
            stream << " ";
        stream << i;
        once = false;
        return *this;
    }
    logger &operator<<(const unsigned char i)
    {
        if(!once)
            stream << " ";
        stream << i;
        once = false;
        return *this;
    }
    logger &operator<<(const char i)
    {
        if(!once)
            stream << " ";
        stream << i;
        once = false;
        return *this;
    }
    logger &operator<<(void* ptr)
    {
        if(!once)
            stream << " ";
        stream << ptr;
        once = false;
        return *this;
    }
    ostream &getStream(){return stream;}
private:
    ostream &stream;
    bool once = true;
};

} // namespace drumlin

#endif // LOGGER_H
