#ifndef IAPPLICATION_H
#define IAPPLICATION_H

#include <boost/thread.hpp>
using namespace boost;

namespace drumlin {

class Event;

class IApplication
{
public:
    virtual void post(Event *event)=0;
    virtual void stop()=0;
    thread::id getThreadId(){ return this_thread::get_id(); }
};

extern IApplication *iapp;

}

#endif // IAPPLICATION_H
