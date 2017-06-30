#ifndef ERROR_H
#define ERROR_H

#include <pleg.h>
#include <string>
#include <functional>
using namespace std;
#include "metatypes.h"
#include "object.h"
#include "thread.h"
#include "exception.h"
#include "application.h"
using namespace drumlin;

namespace Pleg {

class Thread;

extern bool quietEvents;
#define quietDebug() if(!quietEvents)Debug()
void setQuietEvents(bool keepQuiet);

/**
 * @brief The Type enum from QEvent::Type::User to QEvent::Type::MaxUser
 */
#define EventTypes (None,\
    first,\
    Request_first,\
    Request_close,\
    RequestResponse,\
    Request_last,\
    Bluetooth_first,\
    BluetoothStartThread,\
    BluetoothConnectDevices,\
    BluetoothConnectSources,\
    BluetoothDeviceDisconnected,\
    BluetoothDeviceCheckConnected,\
    BluetoothSourceBadWrite,\
    BluetoothSourceDisconnected,\
    BluetoothSourceWriteConfig,\
    BluetoothSocketWriteConfig,\
    Bluetooth_last,\
\
    Gst_first,\
    GstConnectDevices,\
    GstStreamPort,\
    GstStreamFile,\
    GstStreamEnd,\
    GstRemoveJob,\
    Gst_last,\
\
    Control_first,\
    ControlTest,\
    ControlStartStream,\
    ControlStreamStarted,\
    ControlOutputFrame,\
    Control_last,\
\
    Transform_first,\
    TransformError,\
    Transform_last,\
\
    TracerBlock,\
\
    Thread_first,\
    ThreadWarning,\
    ThreadMessage,\
    ThreadShutdown,\
    ThreadRemove,\
    Thread_last,\
\
    ApplicationClose,\
    ApplicationThreadsGone,\
    ApplicationShutdown,\
    ApplicationRestart,\
\
    last\
)
ENUM(EventType,EventTypes)

/**
 * @brief The Event class
 */
class Event
{
public:
    typedef EventType Type;
public:
    /**
     * @brief type
     * @return Event::Type
     */
    virtual Type type()const{ return type_; }
    /**
     * @brief getName
     * @return string
     */
    string getName()const{ return string; }
    /**
     * @brief Event : empty constructor
     */
    Event():Event(Event::Type::None){ string = "none"; }
    /**
     * @brief Event : only meaningful constructor
     * @param _type Event::Type
     * @param _string const char*
     */
    Event(Type _type,string _string):Event((Event::Type)_type),m_string(_string),m_type(_type){}
    Event(Type type):Event(type,string("")){}
    /**
     * @brief clone : used by thread event filter
     * @return Event*
     */
    virtual Event *clone()
    {
        return new Event(type(),getName());
    }
    /**
     * @brief Event::~Event
     */
    virtual ~Event(){}
    friend ostream operator <<(ostream &stream, const Event &event);
    void punt()const;
    void send(Thread *target)const;
private:
    Type m_type;
    string m_string;
};

/**
 * @brief The QSThreadEvent class
 */
template <typename T>
class ThreadEvent : public Event
{
public:
    T *getPointerVal()const{
        return m_ptr;
    }
    /**
     * @brief template<T> T* getPointer()const;
     */
    template <typename Class = T>
    Class *getPointer()const{
        return dynamic_cast<Class*>(m_ptr);
    }
    /**
     * @brief ThreadEvent : only constructor
     * @param _type Event::Type
     * @param _error const char*
     * @param _thread Thread*
     * @param _pointer void*
     */
    ThreadEvent(Type _type,string _error,T *_pointer = 0) : Event(_type,_error),m_ptr(_pointer)
    {
    }

    /**
     * @brief clone : look out for null pointers!
     * @return Event*
     */
    virtual Event *clone()
    {
        return new ThreadEvent<T>(type(),getName(),getPointer());
    }

    /**
     * @brief Event::send : send the event to a thread queue
     * @param thread Thread*
     */
    void post()const
    {
        Thread *thread;
        Object *receiver(getPointer<Object>());
        ThreadWorker *worker(dynamic_cast<ThreadWorker*>(receiver));
        if(worker){
            thread = worker->getThread();
        }else{
            thread = receiver->thread();
        }
        Debug() << "sending" << getName() << "to" << thread;
        thread->post(const_cast<Event*>(static_cast<const Event*>(this)));
    }
private:
    T *m_ptr;
};

template <> template<>
const char *ThreadEvent<const char>::getPointer<const char>()const;

/**
 * @brief event_cast : specialization for const char
 * @param Event*
 */
template <const char>
ThreadEvent<const char> *event_cast(Event *event)
{
    return static_cast<ThreadEvent<const char>*>(event);
}

/**
 * @brief event_cast : used in eventFilter to dereference the ThreadEvent
 * @param Event*
 */
template <class T>
ThreadEvent<T> *event_cast(const Event *event,T **out = 0)
{
    if(out){
        ThreadEvent<T> *ret = static_cast<ThreadEvent<T>*>(const_cast<Event*>(event));
        *out = ret->getPointer();
        return ret;
    }
    return static_cast<ThreadEvent<T>*>(const_cast<Event*>(event));
}

/**
 * @brief event_cast : used to stash something in an event for sending to eventFilter
 * @param _type Event::Type
 * @param error const char*
 * @param that T*
 */
template <const char>
const ThreadEvent<const char> *make_event(Event::Type _type,const char *error,const char *that = 0)
{
    ThreadEvent<const char> *event(new ThreadEvent<const char>(_type,error,that));
    if(that && ((qint64)that > 10 || (qint64)that < 0)){
        quietDebug() << event << event->type() << "from" << this_thread::get_id() << "***" << error << "holds" << that;
    }else{
        quietDebug() << event << event->type() << "from" << this_thread::get_id() << "***" << error;
    }
    return event;
}

/**
 * @brief event_cast : used to stash something in an event for sending to eventFilter
 * @param _type Event::Type
 * @param error const char*
 * @param that T*
 */
template <class T = Object>
const ThreadEvent<T> *make_event(Event::Type _type,const char *error,T *that = 0)
{
    ThreadEvent<T> *event(new ThreadEvent<T>(_type,error,that));
    if(that && ((qint64)that > 10 || (qint64)that < 0)){
        quietDebug() << event << event->type() << "from" << this_thread::get_id() << "***" << error << "holds" << that;
    }else{
        quietDebug() << event << event->type() << "from" << this_thread::get_id() << "***" << error;
    }
    return event;
}

/**
 * @brief The ThreadEvent class
 */
template <typename T>
class PodEvent : public Event
{
public:
    T &getVal(){
        return m_ptr;
    }
    /**
     * @brief ThreadEvent : only constructor
     * @param _type Event::Type
     * @param _error const char*
     * @param _thread Thread*
     * @param _payload T
     */
    PodEvent(Type _type,string _error,T _payload) : Event(_type,_error),m_ptr(_payload)
    {
    }

    /**
     * @brief ThreadEvent : only constructor
     * @param _type Event::Type
     * @param _error const char*
     * @param _thread Thread*
     * @param _payload T
     */
    PodEvent(Type _type,string _error,T &&_payload) : Event(_type,_error),m_ptr(std::move(_payload))
    {
    }

    /**
     * @brief clone : look out for null pointers!
     * @return Event*
     */
    virtual Event *clone()
    {
        T _ptr(m_ptr);
        return new PodEvent<T>(type(),getName(),_ptr);
    }
private:
    T m_ptr;
};

/**
 * @brief event_cast : used to stash something in an event for sending to eventFilter
 * @param _type Event::Type
 * @param error const char*
 * @param that T*
 */
template <class T>
const PodEvent<T> *make_pod_event(Event::Type _type,const char *error,T that)
{
    PodEvent<T> *event(new PodEvent<T>(_type,error,that));
    quietDebug() << event << event->type() << "from" << this_thread::get_id() << "***" << error << "holds" << &event->getVal();
    return event;
}

/**
 * @brief event_cast : used in eventFilter to dereference the ThreadEvent
 * @param Event*
 */
template <class T>
PodEvent<T> *pod_event_cast(const Event *event,T *out = 0)
{
    if(out){
        PodEvent<T> *ret = static_cast<PodEvent<T>*>(const_cast<Event*>(event));
        *out = ret->getVal();
        return ret;
    }
    return static_cast<PodEvent<T>*>(const_cast<Event*>(event));
}

#endif // ERROR_H
