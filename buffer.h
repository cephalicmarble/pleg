#ifndef BUFFERCACHE_H
#define BUFFERCACHE_H

#include "tao_forward.h"
using namespace tao;
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
using namespace boost;
#include <list>
#include <vector>
#include <map>
#include <memory>
#include <utility>
using namespace std;
#include "relevance.h"
#include "mutexcall.h"
#include "exception.h"
using namespace drumlin;
using namespace Pleg;
#include "glib.h"
#ifdef _WIN32
#include <windows.h>
#define PAGE_SHIFT              12L
#else
#include <sys/user.h>
#endif
/**
 * Forward declarations
 */
namespace Pleg {

namespace Sources {
    class Source;
    class GStreamerSampleSource;
    class GStreamerOffsetSource;
}

namespace Buffers {

class Buffer;
class SourceBuffer;
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
typedef std::map<Sources::Source*,heap_t*> buffer_heap_type;
/**
 * @brief The Allocator class : represents chunks of memory
 */
class Allocator
{
protected:
    typedef buffer_heap_type heap_type;
    heap_type heaps;
    int do_unregisterHeap(heap_type::value_type &pair);
public:
    recursive_mutex mutex;
    Allocator();
    ~Allocator();
    int registerSource(Sources::Source *source);
    int unregisterSource(Sources::Source *source);
    int unregisterSources(int);
    const heap_t *getHeap(const Sources::Source *source);
    void *alloc(Buffers::SourceBuffer *buffer);
    int free(Buffers::SourceBuffer *buffer);
    int getStatus(json::value *status);
};

typedef mutex_call_1<Allocator,int,Sources::Source*> registerSource_t;
typedef mutex_call_1<Allocator,int,Sources::Source*> unregisterSource_t;
typedef mutex_call_1<Allocator,int,int> unregisterSources_t;
typedef mutex_call_1<Allocator,const heap_t*,const Sources::Source*> getHeap_t;
typedef mutex_call_1<Allocator,void*,SourceBuffer*> alloc_t;
typedef mutex_call_1<Allocator,int,SourceBuffer*> free_t;
typedef mutex_call_1<Allocator,int,json::value*> getAllocatorStatus_t;

extern registerSource_t registerSource;
extern unregisterSource_t unregisterSource;
extern unregisterSources_t unregisterSources;
extern getHeap_t getHeap;
extern alloc_t alloc;
extern free_t free;
extern getAllocatorStatus_t getAllocatorStatus;

extern Allocator allocator;

/**
 * @brief The Buffer class : represents a chunk of sample
 */
class Buffer
{
public:
    typedef void* ptr_type;
protected:
    ptr_type m_data;
    guint32 m_len;
    Relevance relevance;
    posix_time::ptime timestamp;
    guint32 ttl;
public:
    Buffer();
    virtual ~Buffer();
    bool isValid()const{ return !!m_data; }
    /**
     * @brief getRelevance
     * @return Relevance*
     */
    Relevance *getRelevance()const{ return const_cast<Relevance*>(&relevance); }
    /**
     * @brief getRelevanceRef
     * @return Relevance&
     */
    const Relevance &getRelevanceRef()const{ return relevance; }

    const posix_time::ptime &getTimestampRef()const{ return timestamp; }

    void TTL(guint32 msecs);
    virtual void flush(){}

    /**
     * template<class T> const T*data();
     */
    template <class T>
    const T *data()const{ return (const T*)m_data; }
    guint32 length()const;

    virtual bool isDead()const;
    virtual operator string()const;
    virtual operator byte_array()const;

    friend class Sources::GStreamerSampleSource;
    friend class Sources::GStreamerOffsetSource;
    friend class Allocator;
};

/**
 * @brief The SourceBuffer class : has come straight from the sensor implementation
 */
class SourceBuffer :
    public Buffer
{
public:
    Sources::Source *source;
    SourceBuffer(Sources::Source *_source);
    virtual ~SourceBuffer();
};

/**
 * @brief The Acceptor class
 */
class Acceptor
{
public:
    virtual void accept(const Buffer *buffer)=0;
    virtual void flush(const Buffer *buffer)=0;
};

/**
 * @brief buffer_list_type : the buffers
 */
typedef std::list<std::unique_ptr<Buffer>> buffer_list_type;
/**
 * @brief buffer_vec_type : for function arguments
 */
typedef std::vector<Buffer*> buffer_vec_type;
/**
 * @brief subs_map_type : relates a Buffers::Acceptor to a Relevance
 */
typedef std::map<Relevance,Acceptor*> subs_map_type;

std::pair<const Relevance,Buffers::Acceptor*> make_sub(const Relevance &rel,Acceptor *a);

/**
 * @brief The BufferCache class : is our buffer heap
 */
class BufferCache
{
protected:
    Acceptor *acceptor;
    typedef buffer_list_type buffers_type;
    buffers_type buffers;
    typedef subs_map_type subs_type;
    subs_type subscriptions;
protected:
    char *allocate(Sources::Source *source,size_t n = 1);
public:
    mutex m_mutex;
    BufferCache();
    ~BufferCache();
    bool isLocked();
    guint32 addBuffer(const Buffer *buffer);
    guint32 addSourceBuffer(const SourceBuffer *buffer);
    guint32 flushDeadBuffers();
    guint32 clearRelevantBuffers(const Relevance *rel);
    guint32 callSubscribed(const Buffer *buffer,bool priority);
    buffer_vec_type findRelevant(const Relevance *rel);
    buffer_vec_type registerRelevance(subs_map_type::value_type sub);
    int unregisterAcceptor(Acceptor *acceptor);
    int getStatus(json::value *status);
};

/*
 * thread-safe calls
 */
typedef mutex_call_1<BufferCache,guint32,const Buffer*> addBuffer_t;
typedef mutex_call_1<BufferCache,guint32,const SourceBuffer*> addSourceBuffer_t;
typedef mutex_call_1<BufferCache,guint32,const Relevance*> clearRelevantBuffers_t;
typedef mutex_call_1<BufferCache,buffer_vec_type,const Relevance*> findRelevant_t;
typedef mutex_call_1<BufferCache,buffer_vec_type,subs_map_type::value_type> registerRelevance_t;
typedef mutex_call_1<BufferCache,int,Acceptor*> unregisterAcceptor_t;
typedef mutex_call_1<BufferCache,int,json::value *> getCacheStatus_t;

extern addBuffer_t addBuffer;
extern addSourceBuffer_t addSourceBuffer;
extern clearRelevantBuffers_t clearRelevantBuffers;
extern findRelevant_t findRelevant;
extern registerRelevance_t registerRelevance;
extern unregisterAcceptor_t unregisterAcceptor;
extern getCacheStatus_t getCacheStatus;

extern BufferCache cache;

} // namespace Buffers;

/**
 * @brief Allocator : thread-safe heap monster for BufferCache
 * template<class CPS>
 * @return CPS::Return
 */
template <class CPS>
typename CPS::Return Allocator(CPS cps)
{
    while(!Buffers::allocator.mutex.try_lock()){
        this_thread::yield();
    }
    typename CPS::Return ret(cps(&Buffers::allocator));
    Buffers::allocator.mutex.unlock();
    return ret;
}

/**
 * @brief Cache : thread-safe receiver for BufferCache
 * template<class CPS>
 * @return CPS::Return
 */
template <class CPS>
typename CPS::Return Cache(CPS cps)
{
    while(!Buffers::cache.m_mutex.try_lock()){
        this_thread::yield();
    }
    typename CPS::Return ret(cps(&Buffers::cache));
    Buffers::cache.m_mutex.unlock();
    return ret;
}

template <class MemFun>
typename MemFun::result_type Cache(MemFun fun,typename MemFun::second_argument_type &arg)
{
    while(!Buffers::cache.m_mutex.try_lock()){
        this_thread::yield();
    }
    typename MemFun::result_type ret(fun(Buffers::cache,arg));
    Buffers::cache.m_mutex.unlock();
    return ret;
}

} // namespace Pleg;

#endif // BUFFERCACHE_H
