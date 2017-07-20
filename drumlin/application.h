#ifndef APPLICATION_H
#define APPLICATION_H

#include <object.h>
#include "tao_forward.h"
using namespace tao;
#include <boost/thread/mutex.hpp>
#include <boost/thread/sync_queue.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread.hpp>
using namespace boost;
#include "applicationbase.h"
#include "thread.h"
#include "event.h"
#include "cursor.h"
#include "registry.h"
#include "thread.h"
#include "signalhandler.h"

namespace drumlin {

class Terminator;

extern ApplicationBase *iapp;

#define THREADSLOCK lock_guard<mutex> l(const_cast<mutex&>(m_critical_section));

class Terminator
{
    bool restarting = false;
public:
    Terminator(bool _restarting = false):restarting(_restarting),m_thread(&Terminator::run,this){}
    void run()
    {
        if(iapp){
            iapp->stop();
            if(!restarting){
                make_event(DrumlinEventApplicationShutdown,"Terminator::shutdown",(Object*)0)->punt();
            }else{
                make_event(DrumlinEventApplicationRestart,"Terminator::restart",(Object*)0)->punt();
            }
        }
    }
private:
    thread m_thread;
};

template <class DrumlinApplication = Object>
class Application :
        public DrumlinApplication,
        public SignalHandler,
        public ApplicationBase
{
public:
    typedef DrumlinApplication Base;

    Application():DrumlinApplication(0,nullptr){}
    Application(int &argc,char **argv):DrumlinApplication(argc,argv){}
    ~Application()
    {
        threads.reset(new threads_reg_type());
    }

    /**
     * @brief Application<T>::addThread : optionally start a thread as it is added
     * @param thread Thread*
     * @param start bool
     */
    void addThread(Thread *thread)
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
    threads_type findThread(const string &name,ThreadWorker::Type type)
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
     * @brief getThreads
     * @param type ThreadWorker::Type
     * @return
     */
    threads_type getThreads(ThreadWorker::Type type)
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
     * @brief Application<T>::removeThread : remove a thread
     * @param _thread Thread*
     */
    void removeThread(Thread *thread,bool noDelete = false)
    {
        THREADSLOCK
        Debug() << __func__ << thread->getTask();
        threads->remove(thread->getTask(),true);
        if(!noDelete){
            thread->wait();
            delete thread;
        }
        if(0==std::distance(threads->begin(),threads->end())){
            make_event(DrumlinEventApplicationThreadsGone,"threadsGone",(Object*)0)->punt();
        }
    }

    void post(Event *pevent)
    {
        if(terminated){
            if(event(pevent))
                delete pevent;
        }else{
            m_queue << pevent;
        }
    }

    int exec()
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
    /**
     * @brief Application<T>::eventFilter : deal with events
     * @param obj Object*
     * @param event Event*
     * @return bool
     */
    bool event(Event *pevent)
    {
        if(Base::event(pevent)){
            return true;
        }
        try{
            if((guint32)pevent->type() < (guint32)DrumlinEventEvent_first
            || (guint32)pevent->type() > (guint32)DrumlinEventEvent_last){
                return false;
            }
            switch(pevent->type()){
            case DrumlinEventThreadWarning:
            {
                Debug() << pevent->getName();
                break;
            }
            case DrumlinEventThreadRemove:
            {
                removeThread(event_cast<Thread>(pevent)->getPointer());
                break;
            }
            case DrumlinEventApplicationClose:
            {
                shutdown((bool)event_cast<Object>(pevent)->getPointer());
                break;
            }
            case DrumlinEventApplicationThreadsGone:
            {
                Tracer::endTrace();
                if(terminator){
                    post(new Event(DrumlinEventApplicationShutdown));
                }
                break;
            }
            case DrumlinEventApplicationShutdown:
            {
                Debug() << "Terminated...";
                quit();
                break;
            }
            case DrumlinEventApplicationRestart:
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
    /**
     * @brief Application<T>::stop : stop the server
     */
    virtual void stop()
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
    void quit()
    {
        terminated = true;
    }
    void shutdown(bool restarting = false)
    {
        Debug() << "Terminating...";
        terminator = new Terminator(restarting);
    }
    bool handleSignal(int signal)
    {
        if(Tracer::tracer!=nullptr){
            Tracer::endTrace();
        }
        make_event(DrumlinEventApplicationShutdown,"Signal::shutdown",(Object*)(gint64)signal)->punt();
        return true;
    }
protected:
    Terminator *terminator = nullptr;
private:
    bool terminated = false;
    boost::concurrent::sync_queue<Event*> m_queue;
};

} // namespace drumlin

#endif // APPLICATION_H
