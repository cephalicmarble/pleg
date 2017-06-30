#ifndef QSTHREAD_H
#define QSTHREAD_H

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
    boost::thread &getBoostThread(){ return m_thread; }
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
    virtual void event(Event *);
    virtual void quit();
    void post(Event *);
    operator const char*()const;
    friend std::ostream &operator<<(ostream &stream,const Thread &rel);
    friend class ThreadWorker;
private:
    boost::thread m_thread;
    boost::thread::sync_queue<Event*> m_queue;
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

#define ThreadTypes(\
    first,\
    http,\
    bluez,\
    gstreamer,\
    transform,\
    terminator,\
    last\
)
#define ENUM(ThreadType,ThreadTypes)

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
    virtual void getStatus(json::value *status)const{}
public:
    static mutex critical_section;
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
    void start(){ getThread()->start(); }
    void stop();
    ThreadWorker();
    ThreadWorker(string task);
    ThreadWorker(Thread *_thread);
    virtual ~ThreadWorker();
    virtual void shutdown()=0;
    virtual void report(json::value *obj,ReportType type)const;
    virtual void work(Object *sender,Event *reason);
    friend class Server;
private:
    Thread *thread;
    Type type;
};

} // namespace drumlin

#endif // QSTHREAD_H
