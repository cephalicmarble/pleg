#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include <boost/thread/lock_guard.hpp>

#include "application.h"

#include <boost/thread/mutex.hpp>
#include "thread.h"
#include "event.h"
#include "cursor.h"

namespace drumlin {

template<class T>
mutex Application<T>::thread_critical_section;

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

#define THREADSLOCK lock_guard l(&thread_critical_section);

/**
 * @brief Application<T>::addThread : optionally start a thread as it is added
 * @param thread Thread*
 * @param start bool
 */
template <class T>
void Application<T>::addThread(Thread *thread,bool start)
{
    THREADSLOCK
    qDebug() << __func__ << thread->getTask() << *thread;
    threads->add(thread->getTask(),thread->getWorker());
    if(start){
        thread->start();
    }
}

/**
 * @brief Application<T>::findThread : find threads of name or "all" of type
 * @param name string maybe "all"
 * @param type ThreadWorker::ThreadType
 * @return std::vector<Thread*>
 */
template <class T>
threads_type Application<T>::findThread(const string &name,ThreadWorker::ThreadType type)
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
    qDebug() << __func__ << _thread->getTask();
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
    for(quint16 type = (quint16)ThreadWorker::Type::terminator-1;type>(quint16)ThreadWorker::Type::first;type--){
        threads_type threads(getThreads((ThreadWorker::Type)type));
        for(threads_type::value_type &thread : threads){
            thread->terminate();
            thread->wait(1000);
        }
    }
}

template <class T>
Thread *Application<T>::shutdown(bool restarting)
{
    Debug() << "Terminating...";
    terminator = new Terminator(restarting);
    terminator->run();
    return terminator;
}

template <class T>
bool Application<T>::handleSignal(int signal)
{
    if(Tracer::tracer!=nullptr){
        Tracer::endTrace();
    }
    retval = signal;
    shutdown();
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
        if(!_thread->isStarted() || _thread->isFinished())
            continue;
        lock_guard l(&_thread->critical_section);
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
void Application<T>::exec()
{
    Event *event;
    try{
        while(!terminated && !this_thread::interruption_requested()){
            this_thread::interruption_point();
            {
                this_thread::disable_interruption di;
                m_queue.wait_pull(event);
                event(event);
            }
        }
    }catch(thread_interrupted &ti){
        shutdown();
    }
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
    try{
        if((quint32)event->type() < (quint32)Event::Type::first
        || (quint32)event->type() > (quint32)Event::Type::last){
            return false;
        }
        switch(pevent->type()){
        case Event::Type::ThreadWarning:
        {
            Debug() << pevent->getName();
            break;
        }
        case Event::Type::ThreadMessage:
            // return false without processing
            return false;
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
                post(new QSEvent(QSEvent::Type::ApplicationShutdown));
            }
            break;
        }
        case Event::Type::ApplicationShutdown:
        {
            qDebug() << "Terminated...";
            quit();
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
    :restarting(_restarting),m_thread(&run)
{
}

void Terminator::run()
{
    if(app){
        app->stop();
        if(!restarting){
            app->post(new Event(Event::Type::ApplicationShutdown));
        }else{
            app->post(new Event(Event::Type::ApplicationRestart));
        }
    }
}

} // namespace drumlin
