#ifndef IAPPLICATION_H
#define IAPPLICATION_H

#include <tao_forward.h>
using namespace tao;
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
using namespace boost;
#include "thread.h"

namespace drumlin {

class Event;

typedef std::vector<Thread*> threads_type;

class ApplicationBase : public StatusReporter
{
public:
    ApplicationBase(){}
    virtual ~ApplicationBase(){}
    virtual void post(Event *event)=0;
    virtual void stop()=0;
    thread::id getThreadId(){ return this_thread::get_id(); }
    /**
     * @brief Server::getStatus : return a list.join("\n") of running threads
     * @return const char*
     */
    virtual void getStatus(json::value *status)const;
    typedef Registry<ThreadWorker> threads_reg_type;
    mutex m_critical_section;
protected:
    std::unique_ptr<threads_reg_type> threads;
};

extern ApplicationBase *iapp;

}

#endif // IAPPLICATION_H
