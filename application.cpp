#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>
using namespace boost;
#include "application.h"
#include "thread.h"
#include "event.h"
#include "cursor.h"


namespace drumlin {

template<class T>
mutex Application<T>::critical_section;

int null_argc = 0;
template <class T>
Application<T>::Application():T(null_argc,nullptr),SignalHandler()
{
}

template <class T>
Application<T>::Application(int &argc,char **argv):T(argc,argv),SignalHandler()
{
    threads.reset(new threads_reg_type());
}

template <class T>
Application<T>::~Application()
{
}

#define THREADSLOCK lock_guard<mutex> l(critical_section);

/**
 * @brief Application<T>::addThread : optionally start a thread as it is added
 * @param thread Thread*
 * @param start bool
 */
template <class T>
void Application<T>::addThread(Thread *thread)
{
    THREADSLOCK
    Debug() << __func__ << thread->getTask() << *thread;
    threads->add(thread->getTask(),thread->getWorker());
}

/**
 * @brief Application<T>::findThread : find threads of name or "all" of type
 * @param name string maybe "all"
 * @param type ThreadWorker::ThreadType
 * @return std::vector<Thread*>
 */
template <class T>
threads_type Application<T>::findThread(const string &name,ThreadType type)
{
    THREADSLOCK
    threads_type _threads;
    for(auto thread : *threads){
        if(thread.first == name || (name == "all" && thread.second->getType() == type))
            _threads.push_back(thread.second->getThread());
    }
    return _threads;
}

/**
 * @brief Application<T>::removeThread : remove a thread
 * @param _thread Thread*
 */
template <class T>
void Application<T>::removeThread(Thread *_thread,bool noDelete)
{
    THREADSLOCK
    Debug() << __func__ << _thread->getTask();
    threads->remove(_thread->getTask(),true);
    if(!noDelete){
        _thread->wait();
        delete _thread;
    }
    if(0==std::distance(threads->begin(),threads->end())){
        make_event(Event::Type::ApplicationThreadsGone,"threadsGone",(Object*)0)->punt();
    }
}

/**
 * @brief getThreads
 * @param type ThreadWorker::Type
 * @return
 */
template <class T>
threads_type Application<T>::getThreads(ThreadWorker::Type type)
{
    THREADSLOCK
    threads_type found;
    for(auto thread : *threads){
        if(thread.second->getType() == type)
            found.push_back(thread.second->getThread());
    }
    return found;
}

/**
 * @brief Application<T>::stop : stop the server
 */
template <class T>
void Application<T>::stop()
{
    Debug() << this << __func__;
    for(guint16 type = (guint16)ThreadType_terminator-1;type>(guint16)ThreadType_first;type--){
        threads_type threads(getThreads((ThreadWorker::Type)type));
        for(threads_type::value_type &thread : threads){
            thread->terminate();
            thread->wait(1000);
        }
    }
}

template <class T>
void Application<T>::shutdown(bool restarting)
{
    Debug() << "Terminating...";
    terminator = new Terminator(restarting);
}

template <class T>
bool Application<T>::handleSignal(int signal)
{
    if(Tracer::tracer!=nullptr){
        Tracer::endTrace();
    }
    make_event(Event::Type::ApplicationShutdown,"Signal::shutdown",(Object*)(gint64)signal)->punt();
    return true;
}

/**
 * @brief Server::getStatus : return a list.join("\n") of running threads
 * @return const char*
 */
template <class T>
void Application<T>::getStatus(json::value *status)const
{
    THREADSLOCK
    json::value array(json::empty_array);
    for(threads_reg_type::value_type const& thread : *threads){
        json::value obj(json::empty_object);
        Thread *_thread(thread.second->getThread());
        if(!_thread->isStarted() || _thread->isTerminated())
            continue;
        thread.second->writeToObject(&obj);//report the thread
        array.get_array().push_back(obj);
        thread.second->getStatus(status);//report sub-system
    }
    status->get_object().insert({"threads",array});
}

template <class T>
void Application<T>::post(Event *pevent)
{
    if(terminated){
        if(event(pevent))
            delete pevent;
    }else{
        m_queue << pevent;
    }
}

template <class T>
int Application<T>::exec()
{
    Event *pevent;
    try{
        while(!terminated && !this_thread::interruption_requested()){
            this_thread::interruption_point();
            {
                this_thread::disable_interruption di;
                m_queue.wait_pull(pevent);
                if(event(pevent)){
                    delete pevent;
                }else{
                    Critical() << __func__ << "leaking event" << *pevent;
                }
            }
        }
    }catch(thread_interrupted &ti){
        shutdown();
        return 1;
    }catch(...){
        shutdown();
        return 2;
    }
    return 0;
}

template <class T>
void Application<T>::quit()
{
    terminated = true;
}

/**
 * @brief Application<T>::eventFilter : deal with events
 * @param obj Object*
 * @param event Event*
 * @return bool
 */
template <class T>
bool Application<T>::event(Event *pevent)
{
    if(Base::event(pevent)){
        return true;
    }
    try{
        if((guint32)pevent->type() < (guint32)Event_first
        || (guint32)pevent->type() > (guint32)Event_last){
            return false;
        }
        switch(pevent->type()){
        case Event::Type::ThreadWarning:
        {
            Debug() << pevent->getName();
            break;
        }
        case Event::Type::ThreadRemove:
        {
            removeThread(event_cast<Thread>(pevent)->getPointer());
            break;
        }
        case Event::Type::ApplicationClose:
        {
            shutdown((bool)event_cast<Object>(pevent)->getPointer());
            break;
        }
        case Event::Type::ApplicationThreadsGone:
        {
            Tracer::endTrace();
            if(terminator){
                post(new Event(Event::Type::ApplicationShutdown));
            }
            break;
        }
        case Event::Type::ApplicationShutdown:
        {
            Debug() << "Terminated...";
            quit();
            break;
        }
        case Event::Type::ApplicationRestart:
        {
            Debug() << "Restarted...";
            terminated = false;
            exec();
            break;
        }
        default:
            return false;
        }
        return true;
    }catch(Exception &e){
        Debug() << e.what();
    }
    return false;
}

template class Application<PlegApplication>;

Terminator::Terminator(bool _restarting)
    :restarting(_restarting),m_thread(&Terminator::run,this)
{
}

void Terminator::run()
{
    if(app){
        app->stop();
        if(!restarting){
            make_event(Event::Type::ApplicationShutdown,"Terminator::shutdown",(Object*)0)->punt();
        }else{
            make_event(Event::Type::ApplicationRestart,"Terminator::restart",(Object*)0)->punt();
        }
    }
}

IApplication *iapp;

} // namespace drumlin
