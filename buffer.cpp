#include <qs.h>
using namespace QS;
#include <tao/json.hh>
using namespace tao;
#include "buffer.h"
#include <QThread>
#include <QDateTime>
#include <QDebug>
#include "source.h"
#include <unistd.h>

namespace Buffers {

//#define DEBUGHEAP 1
//#define DEBUGCACHE 1

/**
 * @brief QSBuffer::QSBuffer : only constructor, sets timestamp
 */
QSBuffer::QSBuffer() : timestamp(QDateTime::currentDateTime())
{
    m_data = nullptr;
    m_len = 0;
}

QSBuffer::~QSBuffer()
{
}

/**
 * @brief QSBuffer::TTL : sets time-to-live for the buffer
 * @param msecs quint32
 */
void QSBuffer::TTL(quint32 msecs)
{
    ttl = msecs;
}

/**
 * @brief QSBuffer::length
 * @return quint32
 */
quint32 QSBuffer::length()const
{
    return m_len;
}

/**
 * @brief QSSourceBuffer::QSSourceBuffer : allocates buffer with data from source
 * via call to the Allocator
 * @param source Sources::QSSource*
 */
QSSourceBuffer::QSSourceBuffer(Sources::QSSource *_source):source(_source)
{   
    static int c=0;
    if(c++>79){c=0;std::cout.flush();}

    std::cout << ".";

    _source->nextTick();

    if(source->deleting){
        m_len = 0;
        m_data = nullptr;
    }else{
        Allocator(CPS_call_void(Buffers::alloc,this));
    }
}

/**
 * @brief QSSourceBuffer::~QSSourceBuffer : call Allocator for free
 */
QSSourceBuffer::~QSSourceBuffer()
{
    if(m_data)
        Allocator(CPS_call_void(Buffers::free,this));
}

/**
 * @brief QSSourceBuffer::isDead : calculates whether buffer remains alive.
 */
bool QSBuffer::isDead()const
{
    return timestamp.addMSecs(ttl) <= QDateTime::currentDateTime();
}

/**
 * @brief QSBuffer::operator QByteArray
 */
QSBuffer::operator QByteArray()const
{
    return QByteArray::fromRawData(data<char>(),length());
}

/**
 * @brief QSBuffer::operator QString
 */
QSBuffer::operator QString()const
{
    return QString::fromRawData(data<QChar>(),(int)length());
}

/**
 * @brief QSBufferCache::QSBufferCache : only constructor
 */
QSAllocator::QSAllocator():mutex(QMutex::Recursive)
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
            qCritical() << "*****************Heap exhausted!*****************";
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
            qDebug() << "ALLOCATED" << (void*)ptr << ":" << (void*)alptr;
            for(array_t::value_type &pair : blocks){
                qDebug() << (void*)pair.first << ":" << (void*)pair.second;
            }
#endif
            return alptr;
        }else{
            blocks.push_back({ ptr, (char*)-1 });
        }
        ptr += (int)size;
    }while(allocated<total);
    qCritical() << "*****************Heap misaligned!*****************";
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
        qCritical() << "*****************Double free?*****************";

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

int QSAllocator::getStatus(json::value *status)
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
 * @brief QSBufferCache::~QSBufferCache : deletes buffers from the container, without flushing
 */
QSAllocator::~QSAllocator()
{
    QMutexLocker ml(&mutex);
}

/**
 * @brief QSAllocator::registerSource : register a heap for the source, detailed by the source.
 * @param source Sources::QSSource*
 * @return int total size of allocated heap memory
 */
int QSAllocator::registerSource(Sources::QSSource *source)
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
        throw new QSException("ENOMEM");
    void *mem(std::align(alignof(unsigned char),size,_free,m));
    if(!mem)
        throw new QSException("ENOALIGN");
    heaps.insert({source,new heap_t(n,sz,mem,_free,source->getAlign(),m)});
    return m;
}

int QSAllocator::do_unregisterHeap(heap_type::value_type &pair)
{
    pair.first->deleting = true;
    heap_t *h(pair.second);
    if(h->allocated){
        const QSRelevance rel(pair.first);
        Cache(CPS_call_void(Buffers::clearRelevantBuffers,&rel));
    }
    ::free(h->_free);
    int freud(h->total * h->size);
    heaps.erase(pair.first);
    delete h;
    return freud;
}

/**
 * @brief QSAllocator::unregisterSource : *DANGEROUS* free the heap, first calling Cache(clearRelevantBuffers)
 * @param source Sources::QSSource*
 * @return int
 */
int QSAllocator::unregisterSource(Sources::QSSource *source)
{
    return do_unregisterHeap(*heaps.find(source));
}

/**
 * @brief QSAllocator::unregisterSources : *DANGEROUS* free the heap, first calling Cache(clearRelevantBuffers)
 * @return int
 */
int QSAllocator::unregisterSources(int)
{
    int freud(0);
    std::for_each(heaps.begin(),heaps.end(),[&freud,this](buffer_heap_type::value_type &pair){
        freud += do_unregisterHeap(pair);
    });
    return freud;
}

/**
 * @brief QSAllocator::alloc : allocate a chunk from the source's heap
 * @param source Sources::QSSource*
 * @return void*
 */
void *QSAllocator::alloc(Buffers::QSSourceBuffer *buffer)
{
    if(heaps.end()==heaps.find(buffer->source) || buffer->source->deleting)
        return nullptr;
    heap_t *h(heaps.at(buffer->source));
    if(h->allocated >= h->total){
        qCritical() << "******************Overallocated!******************";
        return nullptr;
    }
    buffer->m_len = h->size;
    buffer->m_data = h->alloc();
    return buffer->m_data;
}

/**
 * @brief QSAllocator::free : free a chunk from the source's heap
 * @param args
 * @return
 */
int QSAllocator::free(Buffers::QSSourceBuffer *buffer)
{
    if(heaps.end()==heaps.find(buffer->source) || buffer->source->deleting)
        return 0;
    heap_t *h(heaps.at(buffer->source));
    h->free(const_cast<char*>(buffer->data<char>()));
    return h->size;
}

registerSource_t registerSource(&QSAllocator::registerSource);
unregisterSource_t unregisterSource(&QSAllocator::unregisterSource);
unregisterSources_t unregisterSources(&QSAllocator::unregisterSources);
alloc_t alloc(&QSAllocator::alloc);
free_t free(&QSAllocator::free);
getAllocatorStatus_t getAllocatorStatus(&QSAllocator::getStatus);

QSAllocator allocator;

/**
 * @brief QSBufferCache::QSBufferCache : only constructor
 */
QSBufferCache::QSBufferCache():mutex(QMutex::NonRecursive)
{
}

/**
 * @brief QSBufferCache::~QSBufferCache : deletes buffers from the container, without flushing
 */
QSBufferCache::~QSBufferCache()
{
    QMutexLocker ml(&mutex);
}

/**
 * @brief QSBufferCache::isLocked : check the cache mutex
 * @return bool
 */
bool QSBufferCache::isLocked()
{
    if(mutex.tryLock()){
        mutex.unlock();
        return false;
    }
    return true;
}

/**
 * @brief QSBufferCache::addBuffer : adds buffer to the container
 * @param buffer QSBuffer*
 * @return quint32 number of buffers dealt
 */
quint32 QSBufferCache::addBuffer(const QSBuffer *buffer)
{
#ifdef DEBUGCACHE
    qDebug() << "Cache" << buffer->getRelevanceRef().getSourceName().c_str() << buffer->getTimestampRef().toString() << buffer->operator QString();
#endif
    QSBuffer *buf(const_cast<QSBuffer*>(buffer));
    buffers.push_front(buffers_type::value_type(buf));
    quint32 n = callSubscribed(buffer,false);
    flushDeadBuffers();
    return n;
}

/**
 * @brief QSBufferCache::addBuffer (qv)
 * @param buffer QSSourceBuffer*
 * @return quint32
 */
quint32 QSBufferCache::addSourceBuffer(const QSSourceBuffer *buffer)
{
    return addBuffer(buffer);
}

/**
 * @brief QSBufferCache::flushDeadBuffers : loop within loop to find subscribed transforms and flush the dead buffers
 * @return number of buffers removed
 */
quint32 QSBufferCache::flushDeadBuffers()
{
    quint32 c(0);
    buffers.erase(std::remove_if(buffers.begin(),buffers.end(),[this,&c](buffers_type::value_type &buffer){
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
 * @brief QSBufferCache::clearRelevantBuffers : loop to delete buffers by relevance
 * @return number of buffers removed
 */
quint32 QSBufferCache::clearRelevantBuffers(const QSRelevance *rel)
{
    quint32 c(0);
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
 * @brief QSBufferCache::callSubscribed : loop over subscriptions to find relevant transforms
 * @param buffer QSBuffer
 * @param flush bool
 * @return quint32 number of transforms called
 */
quint32 QSBufferCache::callSubscribed(const QSBuffer* buffer,bool flush)
{
    quint32 c(0);
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
 * @brief QSBufferCache::findRelevant : loop over buffers to find relevant entries
 * @param rel QSRelevance
 * @return Buffers::QSVectorOfBuffers*
 */
buffer_vec_type QSBufferCache::findRelevant(const QSRelevance *rel)
{
    buffer_vec_type relevant;
    for(buffers_type::value_type &buffer : buffers)
    {
#ifdef DEBUGCACHE
    qDebug() << "Cache" << buffer->getRelevanceRef().getSourceName().c_str() << buffer->getTimestampRef().toString() << buffer->operator QString();
#endif
        if(buffer->getRelevanceRef() == *rel){
            qDebug() << buffer.get() << " found relevant";
            relevant.push_back(buffer_vec_type::value_type(buffer.get()));
        }else{
            qDebug() << buffer.get() << " found irrelevant";
        }
    }
    return relevant;
}

/**
 * @brief make_sub : construct a subs_map_type::value_type
 * @param rel QSRelevance
 * @param a Acceptor*
 * @return std::pair<const QSRelevance,Buffers::Acceptor*>
 */
std::pair<const QSRelevance,Buffers::Acceptor*> make_sub(const QSRelevance &rel,Acceptor *a)
{
    return std::make_pair(rel,a);
}

/**
 * @brief QSBufferCache::registerRelevance : subscribe transform
 * @param rel QSRelevance
 * @param transform Acceptor* (see Factory::XXX::create)
 * @return Buffers::QSVectorOfBuffers*
 */
buffer_vec_type QSBufferCache::registerRelevance(subs_map_type::value_type sub)
{
    auto insert(subscriptions.insert(sub));
    flushDeadBuffers();
    if(insert.second){
        return findRelevant(&(*insert.first).first);
    }else{
        return buffer_vec_type(0);
    }
}

int QSBufferCache::unregisterAcceptor(Acceptor *acceptor)
{
    std::vector<QSRelevance> marked;
    for(subs_map_type::value_type & sub : subscriptions){
        if(sub.second != acceptor){
            continue;
        }
        if(marked.end()==std::find(marked.begin(),marked.end(),sub.first)){
            marked.push_back(sub.first);
        }
    }
    int ret(std::distance(marked.begin(),marked.end()));
    for(QSRelevance &mark : marked){
        subscriptions.erase(mark);
    }
    return ret;
}

int QSBufferCache::getStatus(json::value *status)
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

QSBufferCache cache;

/*
 * thread-safe calls
 */

addBuffer_t addBuffer(&QSBufferCache::addBuffer);
addSourceBuffer_t addSourceBuffer(&QSBufferCache::addSourceBuffer);
clearRelevantBuffers_t clearRelevantBuffers(&QSBufferCache::clearRelevantBuffers);
findRelevant_t findRelevant(&QSBufferCache::findRelevant);
registerRelevance_t registerRelevance(&QSBufferCache::registerRelevance);
unregisterAcceptor_t unregisterAcceptor(&QSBufferCache::unregisterAcceptor);
getCacheStatus_t getCacheStatus(&QSBufferCache::getStatus);

}
