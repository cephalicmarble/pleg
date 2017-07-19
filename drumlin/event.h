#ifndef EVENT_H
#define EVENT_H

#include <drumlin.h>
#include <string>
#include <functional>
using namespace std;
#include "glib.h"
#include "metatypes.h"
#include "object.h"
#include "thread.h"
#include "exception.h"
#include "iapplication.h"

#define DrumlinEventTypes (\
    DrumlinEventNone,\
    DrumlinEventEvent_first,\
    DrumlinEventThread_first,\
    DrumlinEventThreadWork,\
    DrumlinEventThreadWarning,\
    DrumlinEventThreadShutdown,\
    DrumlinEventThreadRemove,\
    DrumlinEventThread_last,\
\
    DrumlinEventApplicationClose,\
    DrumlinEventApplicationThreadsGone,\
    DrumlinEventApplicationShutdown,\
    DrumlinEventApplicationRestart,\
    DrumlinEventEvent_last\
)
ENUM(DrumlinEventType,DrumlinEventTypes)

namespace drumlin {

class Thread;

extern bool quietEvents;
#define quietDebug() if(!quietEvents)Debug()
void setQuietEvents(bool keepQuiet);

/**
 * @brief The Event class
 */
class Event
{
public:
    typedef guint32 Type;
public:
    /**
     * @brief type
     * @return DrumlinEventType
     */
    virtual Type type()const{ return m_type; }
    /**
     * @brief getName
     * @return string
     */
    string getName()const{ return m_string; }
    /**
     * @brief Event : empty constructor
     */
    Event():m_type(0),m_string("none"){}
    /**
     * @brief Event : only meaningful constructor
     * @param _type DrumlinEventType
     * @param _string const char*
     */
    Event(Type _type,string _string):m_type(_type),m_string(_string){}
    Event(Type _type):m_type(_type),m_string(""){}
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
    friend ostream &operator <<(ostream &stream, const Event &event);
    friend logger &operator <<(logger &stream, const Event &event);
    void punt()const;
    void send(Thread *target)const;
private:
    Type m_type;
    string m_string;
};

/**
 * @brief The ThreadEvent class
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
     * @param _type DrumlinEventType
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
        Object *receiver(getPointer<Object>());
        ThreadWorker *worker(dynamic_cast<ThreadWorker*>(receiver));
        if(worker){
            Debug() << "sending" << getName() << "to" << worker->getThread()->getBoostThread().get_id();
            worker->getThread()->post(const_cast<Event*>(static_cast<const Event*>(this)));
        }else{
            Debug() << "sending" << getName() << "to" << drumlin::iapp->getThreadId();
            drumlin::iapp->post(const_cast<Event*>(static_cast<const Event*>(this)));
        }
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
 * @param _type DrumlinEventType
 * @param error const char*
 * @param that T*
 */
template <const char>
const ThreadEvent<const char> *make_event(Event::Type _type,const char *error,const char *that = 0)
{
    ThreadEvent<const char> *event(new ThreadEvent<const char>(_type,error,that));
    return event;
}

/**
 * @brief event_cast : used to stash something in an event for sending to eventFilter
 * @param _type DrumlinEventType
 * @param error const char*
 * @param that T*
 */
template <class T = Object>
const ThreadEvent<T> *make_event(Event::Type _type,const char *error,T *that = 0)
{
    ThreadEvent<T> *event(new ThreadEvent<T>(_type,error,that));
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
     * @param _type DrumlinEventType
     * @param _error const char*
     * @param _thread Thread*
     * @param _payload T
     */
    PodEvent(Type _type,string _error,T _payload) : Event(_type,_error),m_ptr(_payload)
    {
    }

    /**
     * @brief ThreadEvent : only constructor
     * @param _type DrumlinEventType
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
 * @param _type DrumlinEventType
 * @param error const char*
 * @param that T*
 */
template <class T>
const PodEvent<T> *make_pod_event(Event::Type _type,const char *error,T that)
{
    PodEvent<T> *event(new PodEvent<T>(_type,error,that));
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

} // namespace drumlin

#endif // ERROR_H
