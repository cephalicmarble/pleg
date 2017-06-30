#ifndef BUFFERCACHE_H
#define BUFFERCACHE_H

#include "tao_forward.h"
using namespace tao;
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <list>
#include <vector>
#include <map>
#include <memory>
#include <utility>
#include "relevance.h"
#include "mutexcall.h"
#include "exception.h"
#ifdef _WIN32
#include <windows.h>
#define PAGE_SHIFT              12L
#else
#include <sys/user.h>
#endif
/**
 * Forward declarations
 */
namespace Sources {
    class QSSource;
    class QSGStreamerSource;
    class QSGStreamerOffsetSource;
}

namespace Buffers {

class QSBuffer;
class QSSourceBuffer;
/**
 * @brief buffer_heap_type : where buffers go to die...
 */
struct heap_t {
    int allocated;
    int total;
    size_t size;
    size_t align;
    void *memory;
    void *_free;
    int max;
    typedef std::list<std::pair<char*,char*>> array_t;
    array_t blocks;
    heap_t(int _total,size_t _size,void *_memory,void *__free,size_t _align,int _max);
    char *alloc();
protected:
    char *_align(void *ptr);
public:
    void free(char *block);
    void toJson(json::value *status);
};
typedef std::map<Sources::QSSource*,heap_t*> buffer_heap_type;
/**
 * @brief The QSAllocator class : represents chunks of memory
 */
class QSAllocator
{
protected:
    typedef buffer_heap_type heap_type;
    heap_type heaps;
    int do_unregisterHeap(heap_type::value_type &pair);
public:
    QMutex mutex;
    QSAllocator();
    ~QSAllocator();
    int registerSource(Sources::QSSource *source);
    int unregisterSource(Sources::QSSource *source);
    int unregisterSources(int);
    void *alloc(Buffers::QSSourceBuffer *buffer);
    int free(Buffers::QSSourceBuffer *buffer);
    int getStatus(json::value *status);
};

typedef mutex_call_1<QSAllocator,int,Sources::QSSource*> registerSource_t;
typedef mutex_call_1<QSAllocator,int,Sources::QSSource*> unregisterSource_t;
typedef mutex_call_1<QSAllocator,int,int> unregisterSources_t;
typedef mutex_call_1<QSAllocator,void*,QSSourceBuffer*> alloc_t;
typedef mutex_call_1<QSAllocator,int,QSSourceBuffer*> free_t;
typedef mutex_call_1<QSAllocator,int,json::value*> getAllocatorStatus_t;

extern registerSource_t registerSource;
extern unregisterSource_t unregisterSource;
extern unregisterSources_t unregisterSources;
extern alloc_t alloc;
extern free_t free;
extern getAllocatorStatus_t getAllocatorStatus;

extern QSAllocator allocator;

/**
 * @brief Allocator : thread-safe heap monster for QSBufferCache
 * template<class CPS>
 * @return CPS::Return
 */
template <class CPS>
typename CPS::Return Allocator(CPS cps)
{
    while(!allocator.mutex.tryLock()){
        QThread::yieldCurrentThread();
    }
    typename CPS::Return ret(cps(&allocator));
    allocator.mutex.unlock();
    return ret;
}

/**
 * @brief The QSBuffer class : represents a chunk of sample
 */
class QSBuffer
{
public:
    typedef void* ptr_type;
protected:
    ptr_type m_data;
    quint32 m_len;
    QSRelevance relevance;
    QDateTime timestamp;
    quint32 ttl;
public:
    QSBuffer();
    virtual ~QSBuffer();
    bool isValid()const{ return !!m_data; }
    /**
     * @brief getRelevance
     * @return QSRelevance*
     */
    QSRelevance *getRelevance()const{ return const_cast<QSRelevance*>(&relevance); }
    /**
     * @brief getRelevanceRef
     * @return QSRelevance&
     */
    const QSRelevance &getRelevanceRef()const{ return relevance; }

    const QDateTime &getTimestampRef()const{ return timestamp; }

    void TTL(quint32 msecs);
    virtual void flush(){}

    /**
     * template<class T> const T*data();
     */
    template <class T>
    const T *data()const{ return (const T*)m_data; }
    quint32 length()const;

    virtual bool isDead()const;
    virtual operator QString()const;
    virtual operator QByteArray()const;

    friend class Sources::QSGStreamerSource;
    friend class Sources::QSGStreamerOffsetSource;
    friend class QSAllocator;
};

/**
 * @brief The QSSourceBuffer class : has come straight from the sensor implementation
 */
class QSSourceBuffer :
    public QSBuffer
{
public:
    Sources::QSSource *source;
    QSSourceBuffer(Sources::QSSource *_source);
    virtual ~QSSourceBuffer();
};

/**
 * @brief The Acceptor class
 */
class Acceptor
{
public:
    virtual void accept(const QSBuffer *buffer)=0;
    virtual void flush(const QSBuffer *buffer)=0;
};

/**
 * @brief buffer_list_type : the buffers
 */
typedef std::list<std::unique_ptr<QSBuffer>> buffer_list_type;
/**
 * @brief buffer_vec_type : for function arguments
 */
typedef std::vector<QSBuffer*> buffer_vec_type;
/**
 * @brief subs_map_type : relates a Buffers::Acceptor to a QSRelevance
 */
typedef std::map<QSRelevance,Acceptor*> subs_map_type;

std::pair<const QSRelevance,Buffers::Acceptor*> make_sub(const QSRelevance &rel,Acceptor *a);

/**
 * @brief The QSBufferCache class : is our buffer heap
 */
class QSBufferCache
{
protected:
    Acceptor *acceptor;
    typedef buffer_list_type buffers_type;
    buffers_type buffers;
    typedef subs_map_type subs_type;
    subs_type subscriptions;
protected:
    char *allocate(Sources::QSSource *source,size_t n = 1);
public:
    QMutex mutex;
    QSBufferCache();
    ~QSBufferCache();
    bool isLocked();
    quint32 addBuffer(const QSBuffer *buffer);
    quint32 addSourceBuffer(const QSSourceBuffer *buffer);
    quint32 flushDeadBuffers();
    quint32 clearRelevantBuffers(const QSRelevance *rel);
    quint32 callSubscribed(const QSBuffer *buffer,bool priority);
    buffer_vec_type findRelevant(const QSRelevance *rel);
    buffer_vec_type registerRelevance(subs_map_type::value_type sub);
    int unregisterAcceptor(Acceptor *acceptor);
    int getStatus(json::value *status);
};

/*
 * thread-safe calls
 */
typedef mutex_call_1<QSBufferCache,quint32,const QSBuffer*> addBuffer_t;
typedef mutex_call_1<QSBufferCache,quint32,const QSSourceBuffer*> addSourceBuffer_t;
typedef mutex_call_1<QSBufferCache,quint32,const QSRelevance*> clearRelevantBuffers_t;
typedef mutex_call_1<QSBufferCache,buffer_vec_type,const QSRelevance*> findRelevant_t;
typedef mutex_call_1<QSBufferCache,buffer_vec_type,subs_map_type::value_type> registerRelevance_t;
typedef mutex_call_1<QSBufferCache,int,Acceptor*> unregisterAcceptor_t;
typedef mutex_call_1<QSBufferCache,int,json::value *> getCacheStatus_t;

extern addBuffer_t addBuffer;
extern addSourceBuffer_t addSourceBuffer;
extern clearRelevantBuffers_t clearRelevantBuffers;
extern findRelevant_t findRelevant;
extern registerRelevance_t registerRelevance;
extern unregisterAcceptor_t unregisterAcceptor;
extern getCacheStatus_t getCacheStatus;

extern QSBufferCache cache;

/**
 * @brief Cache : thread-safe receiver for QSBufferCache
 * template<class CPS>
 * @return CPS::Return
 */
template <class CPS>
typename CPS::Return Cache(CPS cps)
{
    while(!cache.mutex.tryLock()){
        QThread::yieldCurrentThread();
    }
    typename CPS::Return ret(cps(&cache));
    cache.mutex.unlock();
    return ret;
}

template <class MemFun>
typename MemFun::result_type Cache(MemFun fun,typename MemFun::second_argument_type &arg)
{
    while(!cache.mutex.tryLock()){
        QThread::yieldCurrentThread();
    }
    typename MemFun::result_type ret(fun(cache,arg));
    cache.mutex.unlock();
    return ret;
}

}

#endif // BUFFERCACHE_H
