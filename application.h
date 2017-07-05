#ifndef APPLICATION_H
#define APPLICATION_H

#include <object.h>
#include "tao_forward.h"
using namespace tao;
#include <boost/thread/mutex.hpp>
#include <boost/thread/sync_queue.hpp>
using namespace boost;
#include "plegapp.h"
#include "registry.h"
#include "thread.h"
#include "signalhandler.h"

namespace drumlin {

typedef std::vector<Thread*> threads_type;

class Terminator;

class IApplication
{
public:
    virtual void post(Event *event)=0;
    thread::id getThreadId(){ return this_thread::get_id(); }
};

template <class DrumlinApplication = Object>
class Application :
        public DrumlinApplication,
        public SignalHandler,
        public IApplication
{
public:
    typedef DrumlinApplication Base;
    typedef Registry<ThreadWorker> threads_reg_type;

    Application();
    Application(int &argc,char **argv);
    ~Application();

    static mutex thread_critical_section;

    void addThread(Thread *thread);
    threads_type findThread(const string &name,ThreadWorker::Type type);
    threads_type getThreads(ThreadWorker::Type type);
    void removeThread(Thread *thread,bool noDelete = false);

    void post(Event *event);
    int exec();
    bool event(Event *e);
    virtual void stop();
    void quit();
    void shutdown(bool restarting = false);
    virtual void getStatus(json::value *status)const;
    bool handleSignal(int signal);
protected:
    Terminator *terminator = nullptr;
private:
    bool terminated = false;
    std::unique_ptr<threads_reg_type> threads;
    boost::concurrent::sync_queue<Event*> m_queue;
};

class Terminator
{
    bool restarting = false;
public:
    Terminator(bool restarting = false);
    void run();
private:
    thread m_thread;
};

extern IApplication *iapp;

} // namespace drumlin

#endif // APPLICATION_H
