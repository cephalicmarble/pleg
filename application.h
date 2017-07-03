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

template <class DrumlinApplication = Object>
class Application :
        public DrumlinApplication,
        public SignalHandler
{
public:
    typedef DrumlinApplication Base;
    typedef Registry<ThreadWorker> threads_reg_type;

    Application();
    Application(int &argc,char **argv);
    ~Application();

    mutex thread_critical_section;

    void addThread(Thread *thread,bool start = false);
    threads_type findThread(const string &name,ThreadWorker::Type type);
    threads_type getThreads(ThreadWorker::Type type);
    void removeThread(Thread *thread,bool noDelete = false);

    void post(Event *event);
    void exec();
    bool event(Event *e);
    virtual bool eventFilter(Object *obj, Event *event)=0;
    virtual void stop();
    void wait();

    Thread *shutdown(bool restarting = false);
    virtual void getStatus(json::value *status)const;
    bool handleSignal(int signal);
protected:
    Terminator *terminator = nullptr;
private:
    bool terminated = false;
    std::unique_ptr<threads_reg_type> threads;
    boost::thread::sync_queue<Event*> m_queue;
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

} // namespace drumlin

#endif // APPLICATION_H
