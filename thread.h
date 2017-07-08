#ifndef THREAD_H
#define THREAD_H

#include <iostream>
#include <string>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/sync_queue.hpp>
using namespace boost;
#include "pleg.h"
#include "object.h"
#include "metatypes.h"
#include "registry.h"
using namespace drumlin;
#include "glib.h"

#define ThreadTypes (\
    ThreadType_first,\
    ThreadType_http,\
    ThreadType_bluez,\
    ThreadType_gstreamer,\
    ThreadType_transform,\
    ThreadType_terminator,\
    ThreadType_test,\
    ThreadType_last\
)
ENUM(ThreadType,ThreadTypes)

namespace drumlin {

/*
 * Forward declarations
 */
class ThreadWorker;
class Event;
class Server;

class StatusReporter {
    virtual void getStatus(json::value *status)const=0;
};

class ThreadWorker;

/**
 * @brief The QSThread class
 */
class Thread
{
public:
    bool isTerminated(){ return terminated; }
    void terminate();
    static mutex critical_section;
    Thread(string _task);
    Thread(string _task,ThreadWorker *_worker);
    boost::thread const& getBoostThread()const{ return m_thread; }
    /**
     * @brief getWorker
     * @return ThreadWorker*
     */
    ThreadWorker *getWorker()const{ return worker; }
    /**
     * @brief isStarted
     * @return bool
     */
    bool isStarted()const{ return ready; }
    string getName();
    /**
     * @brief getTask
     * @return string
     */
    string getTask()const{ return task; }
    /**
     * @brief setTask
     * @param _task string
     * @return Thread*
     */
    Thread *setTask(string _task){ task = _task; return this; }
    virtual ~Thread();
    double elapsed();

    virtual void run();
    virtual void exec();
    virtual bool event(Event *);
    virtual void quit();
    void post(Event *);
    operator const char*()const;
    friend logger &operator<<(logger &stream,const Thread &rel);
    friend class ThreadWorker;
    void wait(gint64 millis = -1){
        if(millis<0)
            m_thread.join();
        else
            m_thread.try_join_for(chrono::milliseconds(millis));
    }
private:
    boost::thread m_thread;
    boost::concurrent::sync_queue<Event*> m_queue;
    string task;
    ThreadWorker *worker;
    bool ready = false;
    bool deleting = false;
    bool terminated = false;
    /**
     * @brief setWorker
     * @param _worker ThreadWorker*
     */
    void setWorker(ThreadWorker *_worker){ worker = _worker; }
};

/**
 * @brief The QSThreadWorker class
 */
class ThreadWorker :
    public Object,
    public WorkObject,
    public StatusReporter
{
public:
    double elapsed;
    typedef Registry<WorkObject> jobs_type;
protected:
    jobs_type jobs;
public:
    typedef ThreadType Type;
    /**
     * @brief writeToStream
     * @param stream std::ostream&
     */
    virtual void writeToStream(std::ostream &stream)const;
    virtual void writeToObject(json::value *obj)const;
    virtual void getStatus(json::value *)const{}
public:
    static recursive_mutex critical_section;
    /**
     * @brief getThread
     * @return  Thread*
     */
    Thread *getThread()const{ return thread; }
    void signalTermination();
    /**
     * @brief getType
     * @return ThreadType
     */
    Type getType()const{ return type; }
    /**
     * @brief start
     */
    jobs_type const& getJobs()const{ return jobs; }
    void stop();
    ThreadWorker(Type _type,Object *);
    ThreadWorker(Type _type,string task);
    ThreadWorker(Type _type,Thread *_thread);
    virtual ~ThreadWorker();
    virtual void shutdown()=0;
    virtual void report(json::value *obj,ReportType type)const;
    virtual void work(Object *,Event *){}
    friend class Server;
protected:
    Thread *thread;
    Type type;
};

} // namespace drumlin

#endif // THREAD_H
