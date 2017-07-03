#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include "thread.h"
#include "event.h"
#include "application.h"
#include "metatypes.h"
#include <sstream>
#include <sys/time.h>
#ifdef LINUX
#include <sys/resource.h>
#endif
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/sync_queue.hpp>
using namespace boost;

namespace drumlin {

mutex Thread::critical_section;
/**
 * @brief Thread::Thread : construct a new thread, to be Pleg::Server::addThread(*,bool start)-ed
 * @param _task string name
 */
Thread::Thread(string _task) : Thread(),task(_task)
{
}

/**
 * @brief Thread::~Thread
 */
Thread::~Thread()
{
    Debug() << __func__ << this;
    deleting = true;
    if(getWorker())
        delete getWorker();
}

void Thread::terminate()
{
    Debug() << __func__ << this;
    {
        lock_guard l(&critical_section);
        if(terminated)
            return;
        terminated = true;
        quit();
    }
}

/**
 * @brief Thread::elapsed : get thread-specific elapsed time
 * @return quint64
 */
double Thread::elapsed()
{
#ifdef LINUX
    static timespec spec;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID,&spec);
    return spec.tv_nsec / 1000000.0;
#endif
    return 0.0;
}

/**
 * @brief Thread::getName : (string)"<type>:<task>"
 * @return string
 */
string Thread::getName()
{
    return metaEnum<ThreadWorker::Type>().toString(worker->getType()) + ":" + task;
}

/**
 * @brief Thread::event : deal with thread messages
 * @param obj Object*
 * @param event Event*
 * @return bool
 */
bool Thread::event(Event *pevent)
{
    if(!pevent)
        return false;
    quietDebug() << this << __func__ << pevent->type();
    if((quint32)pevent->type() > (quint32)Event::Type::first
            || (quint32)pevent->type() > (quint32)Event::Type::last){
        return false;
    }
    if((quint32)pevent->type() > (quint32)Event::Type::Thread_first
            && (quint32)pevent->type() < (quint32)Event::Type::Thread_last){
        quietDebug() << pevent->getName();
        switch(pevent->type()){
        case Event::Type::ThreadMessage:
        {
            quietDebug() << pevent->type() << "invocation of" << pevent->getName();
            Object *target;
            event_cast<Object>(pevent,&target);
            target->metaObject()->invokeMethod(target,pevent->getName().c_str(),Qt::DirectConnection);
            break;
        }
        case QSEvent::Type::ThreadShutdown:
        {
            quietDebug() << pevent->type() << "invocation of" << pevent->getName();
            quit();
            break;
        }
        default:
            quietDebug() << pevent->type() << __func__ <<  "unimplemented";
            return false;
        }
        return true;
    }
    //        quietDebug() << "signalling" << pevent << "in" << QThread::currentThread() << "***" << pevent->getName();
    //        event->setAccepted(QMetaMethod::fromSignal(&ThreadWorker::signalEventFilter)
    //                           .invoke(getWorker(),Qt::DirectConnection,Q_ARG(QObject*,this),Q_ARG(QSEvent*,pevent->clone())));
    //        quietDebug() << "accepted?" << event->isAccepted();
    return false;
}

/**
 * @brief Thread::run : thread function
 * Sits and processes events, yielding to interruption and delayed stoppage from stop().
 * Installs the thread as its own eventFilter (qv)
 * Signals getWorker()->run(Object*,Event*) to do work.
 */
void Thread::run()
{
//    lock_guard l(&critical_section);
//    getWorker()->moveToThread(this);
//    l.unlock();
    getWorker()->work(nullptr,nullptr);
    ready = true;
    exec();
    terminate();
    if(getWorker()){
        worker->shutdown();
        delete worker;
        worker = nullptr;
    }
    make_event(Event::Type::ThreadRemove,"removeThread",this)->punt();
}

void Thread::post(Event *event)
{
    m_queue << event;
}

void Thread::exec()
{
    Event *event;
    while(!deleting && !terminated && !m_thread.interruption_requested()){
        try{
            this_thread::interruption_point();
            {
                this_thread::disable_interruption di;
                m_queue.wait_pull(event);
                event(event);
            }
        }catch(thread_interrupted &ti){
            terminate();
        }
    }
}

void Thread::quit()
{
    m_thread.interrupt();
}

void ThreadWorker::stop()
{
    shutdown();
    signalTermination();
}

recursive_mutex ThreadWorker::critical_section;

ThreadWorker::ThreadWorker(Object *parent = nullptr) : Object(parent)
{
}

ThreadWorker::ThreadWorker(string task) : Object()
{
    thread = new Thread(task);
    thread->setWorker(this);
}

/**
 * @brief ThreadWorker::ThreadWorker : worker constructor
 * connects thread->finished() to punt event back to application for removal.
 * Sets worker to this, and moves the worker to the (already created) thread.
 * @param _thread Thread*
 */
ThreadWorker::ThreadWorker(Thread *_thread) : Object(),thread(_thread)
{
    thread->setWorker(this);
}

/**
 * @brief ThreadWorker::~ThreadWorker : removes the event filter
 */
ThreadWorker::~ThreadWorker()
{
    thread->removeEventFilter(this);
    thread->worker = nullptr;        
}

/**
 * @brief ThreadWorker::signalTermination
 */
void ThreadWorker::signalTermination()
{
    if(!getThread()->isTerminated())
        getThread()->terminate();
}

void ThreadWorker::report(json::value *obj,ReportType type)const
{
    auto &map(obj->get_object());
    map.insert({"task",getThread()->getTask().toStdString()});
    map.insert({"type",string(metaEnum<Type>().toString(this->type))});
    if(type & QSWorkObject::ReportType::Elapsed){
        map.insert({"elapsed",getThread()->elapsed()});
    }
    if(type & QSWorkObject::ReportType::Memory){
        map.insert({"memory",0});
    }
    if(type & QSWorkObject::ReportType::Jobs){
        if(map.end()==map.find("jobs"))
            map.insert({"jobs",json::empty_object});
        for(jobs_type::value_type const& job : jobs){
            json::value job_obj(json::empty_object);
            job.second->report(&job_obj,type);
            map.at("jobs").get_object().insert({job.first,job_obj});
        }
    }
}

/* STREAM OPERATORS */

void ThreadWorker::writeToObject(json::value *obj)const
{
    report(obj,QSWorkObject::ReportType::All);
    obj->get_object().insert({"task",getThread()->getTask()});
}

void ThreadWorker::writeToStream(std::ostream &stream)const
{
    json::value obj(json::empty_object);
    writeToObject(&obj);
    json::to_stream(stream,obj);
}

/**
 * @brief operator const char*: for QDebug
 */
Thread::operator const char*()const
{
    lock_guard l(&critical_section);
    static char *buf;
    if(buf)free(buf);
    std::stringstream ss;
    ss << *this;
    return buf = strdup(ss.str().c_str());
}

/**
 * @brief operator << : stream operator
 * @param stream std::ostream &
 * @param rhs Thread const&
 * @return std::ostream &
 */
std::ostream &operator<<(std::ostream &stream,const Thread &rel)
{
    rel.getWorker()->writeToStream(stream);
    return stream;
}

/**
 * @brief operator << : stream operator
 * @param stream QDebug&
 * @param rhs Thread const&
 * @return QDebug&
 */
QDebug &operator<<(QDebug &stream,const Thread &rhs)
{
    stream << string(rhs);
    return stream;
}

} // namespace drumlin
