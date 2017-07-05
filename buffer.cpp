#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include "buffer.h"
#include "source.h"
#include "byte_array.h"
#include <unistd.h>
using namespace boost;

namespace Pleg {

namespace Buffers {

//#define DEBUGHEAP 1
//#define DEBUGCACHE 1

/**
 * @brief Buffer::Buffer : only constructor, sets timestamp
 */
Buffer::Buffer() : timestamp(posix_time::microsec_clock::universal_time())
{
    m_data = nullptr;
    m_len = 0;
}

Buffer::~Buffer()
{
}

/**
 * @brief Buffer::TTL : sets time-to-live for the buffer
 * @param msecs quint32
 */
void Buffer::TTL(guint32 msecs)
{
    ttl = msecs;
}

/**
 * @brief Buffer::length
 * @return quint32
 */
guint32 Buffer::length()const
{
    return m_len;
}

/**
 * @brief SourceBuffer::SourceBuffer : allocates buffer with data from source
 * via call to the Allocator
 * @param source Sources::Source*
 */
SourceBuffer::SourceBuffer(Sources::Source *_source):source(_source)
{   
    static int c=0;
    if(c++>79){c=0;std::cout.flush();}

    std::cout << ".";

    _source->nextTick();

    if(source->deleting){
        m_len = 0;
        m_data = nullptr;
    }else{
        Pleg::Allocator(CPS_call_void(Buffers::alloc,this));
    }
}

/**
 * @brief SourceBuffer::~SourceBuffer : call Allocator for free
 */
SourceBuffer::~SourceBuffer()
{
    if(m_data)
        Pleg::Allocator(CPS_call_void(Buffers::free,this));
}

posix_time::ptime Buffer::advanceClock()
{
    timestamp += posix_time::milliseconds(ttl);
    return timestamp;
}

/**
 * @brief SourceBuffer::isDead : calculates whether buffer remains alive.
 */
bool Buffer::isDead()const
{
    return timestamp <= posix_time::microsec_clock::universal_time();
}

/**
 * @brief Buffer::operator byte_array
 */
Buffer::operator byte_array()const
{
    return byte_array::fromRawData(m_data,m_len);
}

/**
 * @brief Buffer::operator string
 */
Buffer::operator string()const
{
    return string(data<char>(),(int)length());
}

/**
 * @brief BufferCache::BufferCache : only constructor
 */
Allocator::Allocator()
{
}

/**
 * @brief heap_t::heap_t : prepare a heap
 * @param _total int
 * @param _size size_t
 * @param _memory void*
 * @param _align size_t
 */
heap_t::heap_t(int _total,size_t _size,void *_memory,void *__free,size_t _align,int _max)
    :allocated(0),total(_total),size(_size),align(_align),memory(_memory),_free(__free),max(_max)
{
#ifdef DEBUGHEAP
    qDebug() << "HEAP" << total << "of" << size;
#endif
}

/**
 * @brief heap_t::alloc : allocate a previously agreed upon sized chunk from this heap
 *
 * The heap keeps a list of marked pairs of pointers. The second is a pointer to an aligned
 * block of previously agreed upon size. The first is nullptr when not allocated (freed),
 * 1 when reallocated, and ==second when first allocated.
 *
 * @return char*
 */
char *heap_t::alloc()
{
    char *ptr,*alptr = nullptr;
    array_t::iterator it(std::find_if(blocks.begin(),blocks.end(),[&alptr](array_t::value_type &pair){
        if(pair.second != (char*)-1) {
            if(pair.first == nullptr) {
                return true;
            }else{
                alptr = pair.second;
            }
        }
        return false;
    }));
    if(it != blocks.end()){
#ifdef DEBUGHEAP
        qDebug() << "REALLOCATED" << (void*)(*it).first << ":" << (void*)(*it).second;
#endif
        (*it).first = (char*)1;
        allocated++;
        return (*it).second;
    }else{
        if(std::distance(blocks.begin(),blocks.end()) >= total){
            Critical() << "*****************Heap exhausted!*****************";
            return nullptr;
        }
        ptr = alptr;
    }
    if(blocks.begin() == it){
        ptr = (char*)memory;
    }else{
        ptr += (int)size;
    }
    do{
        allocated++;
        alptr = _align(ptr);
        if(alptr){
            blocks.push_back({ ptr, alptr });
#ifdef DEBUGHEAP
            Debug() << "ALLOCATED" << (void*)ptr << ":" << (void*)alptr;
            for(array_t::value_type &pair : blocks){
                Debug() << (void*)pair.first << ":" << (void*)pair.second;
            }
#endif
            return alptr;
        }else{
            blocks.push_back({ ptr, (char*)-1 });
        }
        ptr += (int)size;
    }while(allocated<total);
    Critical() << "*****************Heap misaligned!*****************";
    return nullptr;
}

/**
 * @brief heap_t::_align : call std::align to align the passed pointer
 * @param ptr char*
 * @return char*
 */
char *heap_t::_align(void *ptr) {
    size_t memsz(size*(total-allocated));
    void *mem((void*)ptr);
    return (char*)std::align(align,size,mem,memsz);
}

/**
 * @brief heap_t::free : free a block by marking its pair
 * @param block char*
 */
void heap_t::free(char *block)
{
    array_t::iterator it(std::find_if(blocks.begin(),blocks.end(),[block](array_t::value_type &pair){
        return pair.second == block && pair.first != nullptr;
    }));
    if(it == blocks.end()){
#ifdef DEBUGHEAP
        for(array_t::value_type &pair : blocks){
            qDebug() << (void*)pair.first << ":" << (void*)pair.second;
        }
#endif
        Critical() << "*****************Double free?*****************";

    }
    allocated--;
#ifdef DEBUGHEAP
    qDebug() << "FREED" << (void*)(*it).first << ":" << (void*)(*it).second;
#endif
    (*it).first = nullptr;
#ifdef DEBUGHEAP
    for(array_t::value_type &pair : blocks){
        qDebug() << (void*)pair.first << ":" << (void*)pair.second;
    }
#endif
}

void heap_t::toJson(json::value *heap)
{
    json::object_t &obj(heap->get_object());

    obj.insert({"allocated",allocated});
    obj.insert({"total",total});
    obj.insert({"size",size});
    obj.insert({"align",align});
}

int Allocator::getStatus(json::value *status)
{
    json::value allocator(json::empty_object);
    json::object_t &obj(allocator.get_object());

    json::value _heaps(json::empty_array);
    json::array_t &array(_heaps.get_array());
    for(heap_type::value_type const& heap : heaps){
        json::value _heap(json::empty_object);
        _heap.get_object().insert({"name",(const char*)*heap.first});
        heap.second->toJson(&_heap);
        array.push_back(_heap);
    }
    obj.insert({"heaps",_heaps});
    status->get_object().insert({"allocator",allocator});
    return 0;
}

/**
 * @brief BufferCache::~BufferCache : deletes buffers from the container, without flushing
 */
Allocator::~Allocator()
{
    lock_guard<recursive_mutex> l(mutex);
}

/**
 * @brief Allocator::registerSource : register a heap for the source, detailed by the source.
 * @param source Sources::Source*
 * @return int total size of allocated heap memory
 */
int Allocator::registerSource(Sources::Source *source)
{
    size_t sz(source->sizeT());
    int n(4 + (source->getTTL()/source->getTau()));
    size_t m(sz*n),tmp = ceil(m / (1<<PAGE_SHIFT));
    if(tmp > 1){
        m = (1 + tmp) * (1<<PAGE_SHIFT);
    }
    size_t size(m);
    void *_free(malloc(m));
    if(!_free)
        throw new Exception("ENOMEM");
    void *mem(std::align(alignof(unsigned char),size,_free,m));
    if(!mem)
        throw new Exception("ENOALIGN");
    heaps.insert({source,new heap_t(n,sz,mem,_free,source->getAlign(),m)});
    return m;
}

int Allocator::do_unregisterHeap(heap_type::value_type &pair)
{
    pair.first->deleting = true;
    heap_t *h(pair.second);
    if(h->allocated){
        const Relevance rel(pair.first);
        Cache(CPS_call_void(Buffers::clearRelevantBuffers,&rel));
    }
    ::free(h->_free);
    int freud(h->total * h->size);
    heaps.erase(pair.first);
    delete h;
    return freud;
}

/**
 * @brief Allocator::unregisterSource : *DANGEROUS* free the heap, first calling Cache(clearRelevantBuffers)
 * @param source Sources::Source*
 * @return int
 */
int Allocator::unregisterSource(Sources::Source *source)
{
    return do_unregisterHeap(*heaps.find(source));
}

/**
 * @brief Allocator::unregisterSources : *DANGEROUS* free the heap, first calling Cache(clearRelevantBuffers)
 * @return int
 */
int Allocator::unregisterSources(int)
{
    int freud(0);
    std::for_each(heaps.begin(),heaps.end(),[&freud,this](buffer_heap_type::value_type &pair){
        freud += do_unregisterHeap(pair);
    });
    return freud;
}

/**
 * @brief Allocator::unregisterSource : *DANGEROUS* fetch the heap
 * @param source Sources::Source*
 * @return int
 */
const heap_t *Allocator::getHeap(const Sources::Source *source)
{
    return heaps.at(const_cast<Sources::Source*>(source));
}

/**
 * @brief Allocator::alloc : allocate a chunk from the source's heap
 * @param source Sources::Source*
 * @return void*
 */
void *Allocator::alloc(Buffers::SourceBuffer *buffer)
{
    if(heaps.end()==heaps.find(buffer->source) || buffer->source->deleting)
        return nullptr;
    heap_t *h(heaps.at(buffer->source));
    if(h->allocated >= h->total){
        Critical() << "******************Overallocated!******************";
        return nullptr;
    }
    buffer->m_len = h->size;
    buffer->m_data = h->alloc();
    return buffer->m_data;
}

/**
 * @brief Allocator::free : free a chunk from the source's heap
 * @param args
 * @return
 */
int Allocator::free(Buffers::SourceBuffer *buffer)
{
    if(heaps.end()==heaps.find(buffer->source) || buffer->source->deleting)
        return 0;
    heap_t *h(heaps.at(buffer->source));
    h->free(const_cast<char*>(buffer->data<char>()));
    return h->size;
}

registerSource_t registerSource(&Allocator::registerSource);
unregisterSource_t unregisterSource(&Allocator::unregisterSource);
unregisterSources_t unregisterSources(&Allocator::unregisterSources);
getHeap_t getHeap(&Allocator::getHeap);
alloc_t alloc(&Allocator::alloc);
free_t free(&Allocator::free);
getAllocatorStatus_t getAllocatorStatus(&Allocator::getStatus);

Pleg::Buffers::Allocator allocator;

/**
 * @brief BufferCache::BufferCache : only constructor
 */
BufferCache::BufferCache()
{
}

/**
 * @brief BufferCache::~BufferCache : deletes buffers from the container, without flushing
 */
BufferCache::~BufferCache()
{
    lock_guard<mutex> l(m_mutex);
}

/**
 * @brief BufferCache::isLocked : check the cache mutex
 * @return bool
 */
bool BufferCache::isLocked()
{
    if(m_mutex.try_lock()){
        m_mutex.unlock();
        return false;
    }
    return true;
}

/**
 * @brief BufferCache::addBuffer : adds buffer to the container
 * @param buffer Buffer*
 * @return quint32 number of buffers dealt
 */
guint32 BufferCache::addBuffer(const Buffer *buffer)
{
#ifdef DEBUGCACHE
    Debug() << "Cache" << buffer->getRelevanceRef().getSourceName().c_str() << buffer->getTimestampRef().toString() << buffer->operator string();
#endif
    Buffer *buf(const_cast<Buffer*>(buffer));
    buffers.push_front(buffers_type::value_type(buf));
    guint32 n = callSubscribed(buffer,false);
    flushDeadBuffers();
    return n;
}

/**
 * @brief BufferCache::addBuffer (qv)
 * @param buffer SourceBuffer*
 * @return quint32
 */
guint32 BufferCache::addSourceBuffer(const SourceBuffer *buffer)
{
    return addBuffer(buffer);
}

/**
 * @brief BufferCache::flushDeadBuffers : loop within loop to find subscribed transforms and flush the dead buffers
 * @return number of buffers removed
 */
guint32 BufferCache::flushDeadBuffers()
{
    guint32 c(0);
    buffers.erase(std::remove_if(buffers.begin(),buffers.end(),[this,&c](buffers_type::value_type &buffer){
        buffer->advanceClock();
        if(buffer->isDead()){
            callSubscribed(buffer.get(),true);
            c++;
            return true;
        }
        return false;
    }),buffers.end());
    for(subs_type::value_type &sub : subscriptions){
        sub.second->flush(nullptr);
    }
    return c;
}

/**
 * @brief BufferCache::clearRelevantBuffers : loop to delete buffers by relevance
 * @return number of buffers removed
 */
guint32 BufferCache::clearRelevantBuffers(const Relevance *rel)
{
    guint32 c(0);
    buffers.erase(std::remove_if(buffers.begin(),buffers.end(),[this,&c,rel](buffers_type::value_type &buffer){
        if(buffer->getRelevanceRef() == *rel) {
            c++;
            return true;
        }
        return false;
    }),buffers.end());
    return c;
}

/**
 * @brief BufferCache::callSubscribed : loop over subscriptions to find relevant transforms
 * @param buffer Buffer
 * @param flush bool
 * @return guint32 number of transforms called
 */
guint32 BufferCache::callSubscribed(const Buffer* buffer,bool flush)
{
    guint32 c(0);
    for(subs_type::value_type &sub : subscriptions)
    {
        if(buffer->getRelevanceRef() == sub.first){
            if(flush){
                sub.second->flush(buffer);
            }else{
                sub.second->accept(buffer);
            }
            c++;
        }
    }
    return c;
}

/**
 * @brief BufferCache::findRelevant : loop over buffers to find relevant entries
 * @param rel Relevance
 * @return Buffers::VectorOfBuffers*
 */
buffer_vec_type BufferCache::findRelevant(const Relevance *rel)
{
    buffer_vec_type relevant;
    for(buffers_type::value_type &buffer : buffers)
    {
#ifdef DEBUGCACHE
    Debug() << "Cache" << buffer->getRelevanceRef().getSourceName().c_str() << buffer->getTimestampRef().toString() << buffer->operator string();
#endif
        if(buffer->getRelevanceRef() == *rel){
            Debug() << buffer.get() << " found relevant";
            relevant.push_back(buffer_vec_type::value_type(buffer.get()));
        }else{
            Debug() << buffer.get() << " found irrelevant";
        }
    }
    return relevant;
}

/**
 * @brief make_sub : construct a subs_map_type::value_type
 * @param rel Relevance
 * @param a Acceptor*
 * @return std::pair<const Relevance,Buffers::Acceptor*>
 */
std::pair<const Relevance,Buffers::Acceptor*> make_sub(const Relevance &rel,Acceptor *a)
{
    return std::make_pair(rel,a);
}

/**
 * @brief BufferCache::registerRelevance : subscribe transform
 * @param rel Relevance
 * @param transform Acceptor* (see Factory::XXX::create)
 * @return Buffers::VectorOfBuffers*
 */
buffer_vec_type BufferCache::registerRelevance(subs_map_type::value_type sub)
{
    auto insert(subscriptions.insert(sub));
    flushDeadBuffers();
    if(insert.second){
        return findRelevant(&(*insert.first).first);
    }else{
        return buffer_vec_type(0);
    }
}

int BufferCache::unregisterAcceptor(Acceptor *acceptor)
{
    std::vector<Relevance> marked;
    for(subs_map_type::value_type & sub : subscriptions){
        if(sub.second != acceptor){
            continue;
        }
        if(marked.end()==std::find(marked.begin(),marked.end(),sub.first)){
            marked.push_back(sub.first);
        }
    }
    int ret(std::distance(marked.begin(),marked.end()));
    for(Relevance &mark : marked){
        subscriptions.erase(mark);
    }
    return ret;
}

int BufferCache::getStatus(json::value *status)
{
    json::value cache(json::empty_object);
    json::object_t &obj(cache.get_object());

    json::value subs(json::empty_array);
    json::array_t &array(subs.get_array());
    for(subs_type::value_type const& sub : subscriptions){
        json::value _sub(json::empty_object);
        sub.first.toJson(&_sub);
        array.push_back(_sub);
    }
    obj.insert({"subs",subs});
    status->get_object().insert({"cache",cache});
    return 0;
}

BufferCache cache;

/*
 * thread-safe calls
 */

addBuffer_t addBuffer(&BufferCache::addBuffer);
addSourceBuffer_t addSourceBuffer(&BufferCache::addSourceBuffer);
clearRelevantBuffers_t clearRelevantBuffers(&BufferCache::clearRelevantBuffers);
findRelevant_t findRelevant(&BufferCache::findRelevant);
registerRelevance_t registerRelevance(&BufferCache::registerRelevance);
unregisterAcceptor_t unregisterAcceptor(&BufferCache::unregisterAcceptor);
getCacheStatus_t getCacheStatus(&BufferCache::getStatus);

} // namespace Buffers

} // namespace Pleg
