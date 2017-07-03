#ifndef SOURCE_H
#define SOURCE_H

#include <pleg.h>
using namespace Pleg;
#include "tao_forward.h"
using namespace tao;
#include <string>
#include <map>
using namespace std;
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio.hpp>
#include <boost/uuid/uuid.hpp>
using namespace boost;
#include "object.h"
#include "mutexcall.h"
#include "buffer.h"
#include "registry.h"
using namespace drumlin;
using namespace Pleg;

class Request;
//class BluetoothLEDevice;
#include "gstreamer.h"

#define SourceTypes (\
    HeartRate,\
    BatteryLevel,\
    Mock,\
    Null,\
    Gstreamer,\
    Offset\
)
ENUM(SourceType,SourceTypes)

namespace Sources {

/**
 * @brief The Source class : represents an abstract data source
 */
class Source :
    public Object,
    public WorkObject
{
public:
    typedef SourceType Type;

    Source(std::string const& _name):Object(),name(_name){}
    Source():Object(),name("source"){}
    virtual ~Source();
    virtual void stop(){}
    virtual guint64 nextTick(){
        return ++tick;
    }
    string const& getName()const { return name; }
    void setMemory(size_t _memory){ memory = _memory; }

    /**
     * @brief Source::writeNextValue
     * @param bytes void*
     * @param len quint32
     */
    virtual void writeNext(void *bytes, quint32 len){}
    virtual void writeNext();
    virtual guint32 lengthData(){ return 0; }
    virtual guint32 getTTL(){ return 2000; }
    virtual guint32 getTau(){ return 1000; }
    virtual size_t getAlign(){ return 1; }
    virtual guint32 sizeT(){ return 0; }

    virtual bool isDestructing(){return false;}
    virtual uuids::uuid getUuid(){ return uuids::uuid(); }

    operator const char*()const{ return name.c_str(); }
    virtual void report(json::value *obj,ReportType type)const;

    virtual void writeToFile(Request *,string rpath);

    Type type;
    bool deleting = false;
private:
    guint64 tick = 0;
    size_t memory;
    string name;
};

/**
 * @brief The Source class : represents an abstract data source
 */
template <size_t _len = 512>
class BufferedSource :
    public Source
{
protected:
    guint8 data[_len];
    const guint32 maxlen = _len;
    guint32 len;
    uuids::uuid uuid;
public:
    /**
     * @brief BufferedSource : default constructor
     */
    BufferedSource(std::string const& name):Source(name){}

    guint8 *_data(){ return data; }
    /**
     * @brief BufferedSource::writeNext : buffer next sample
     */
    virtual void writeNextValue(const byte_array &value, const uuids::uuid &_uuid)
    {
        uuid = _uuid;
        memcpy((void*)data,(void*)value.data(),len = qMin((guint32)value.length(),maxlen));
    }

    /**
     * @brief BufferedSource::lengthData : return the buffered sample length
     * @return quint32
     */
    virtual guint32 lengthData()
    {
        return len;
    }

    virtual guint32 sizeT()
    {
        return maxlen;
    }

    /**
     * @brief read : from the input buffer
     * @param buf void*
     * @param len guint32
     * @return guint32
     */
    virtual guint32 read(void *buf,guint32 len)
    {
        if(!lengthData())
            return 0;
        memcpy(buf,(void*)data,lengthData());
        return lengthData();
    }

    /**
     * @brief getUuid
     * @return QUuid
     */
    virtual uuids::uuid getUuid(){ return uuid; }

    virtual size_t getAlign(){ return alignof(data); }

    /**
     * @brief operator const char *
     */
    virtual operator const char*(){ return "buffered"; }
};

template <class T>
class TypedSource :
    public Source
{
protected:
    T t;
    uuids::uuid uuid;
public:
    /**
     * @brief TypedSource : default constructor
     */
    TypedSource(string const& name):Source(name){}
    /**
     * @brief getData
     * @return T* where T=the buffered data type (see HeartRateSource)
     */
    T*getData(){ return &t; }
    virtual operator const char*(){ return "typed"; }
    /**
     * @brief TypedSource<T>::lengthData : return the buffered sample length
     * @return quint32
     */
    virtual guint32 lengthData()
    {
        return sizeT();
    }

    virtual guint32 sizeT()
    {
        return sizeof(T);
    }

    /**
     * @brief TypedSource<T>::writeNextValue
     * @param bytes void*
     * @param len quint32
     */
    void writeNext(void *bytes, guint32 len)
    {
        Buffers::SourceBuffer *buf(new Buffers::SourceBuffer(this));
        memcpy(getData()->data,bytes,len);
        buf->getRelevance()->setSource(this);
        buf->TTL(getTTL());
        Buffers::Cache(CPS_call_void(Buffers::addSourceBuffer,const_cast<const Buffers::SourceBuffer*>(buf)));
    }

    /**
     * @brief getUuid
     * @return QUuid
     */
    virtual uuids::uuid getUuid(){ return uuid; }

    virtual size_t getAlign(){ return alignof(T); }
};

class SourceRegistry :
    public Registry<Source>
{
public:
    /**
     * @brief add : TBC from Server only, inserts a new tring Source* mapping
     * @param str tring
     * @param src Source*
     */
    void add(const string &str,Source *src)
    {
        lock_guard l(&mutex);
        src->setMemory(Allocator(CPS_call_void(Buffers::registerSource,src)));
        Registry<Source>::add(str,src);
    }

    /**
     * @brief remove : remove a tring Source* mapping, delete the Source
     * @param str
     */
    void remove(const string &str)
    {
        lock_guard l(&mutex);
        Allocator(CPS_call_void(Buffers::unregisterSource,map->at(str)));
        Registry<Source>::remove(str);
    }

    /**
     * @brief removeAll : remove a tring Source* mapping, delete the Source
     * @param str
     */
    void removeAll()
    {
        lock_guard l(&mutex);
        Allocator(CPS_call_void(Buffers::unregisterSources,0));
        Registry<Source>::removeAll();
    }
    friend void Sources::getStatus(json::value*);
};

/**
 * @brief sources_type : map of named sources
 */
typedef SourceRegistry sources_type;
extern sources_type sources;

void getStatus(json::value *status);

/**
 * @brief The MockSource class : enumerates the alphabet into a buffer
 */
class MockSource :
    public BufferedSource<32>
{
    typedef BufferedSource<32> Base;
public:
    MockSource();
    void start();
    virtual void writeNext(void *buf,guint32 len);
    virtual uuids::uuid getUuid(){ return Pleg::string_gen("1"); }
    void stop();
    void timedOut(const system::error_code& e);
private:
    asio::deadline_timer timer;
};

/**
 * @brief The NullSource class : does nothing.
 */
class NullSource : public Source
{
public:
    NullSource():Source("null"){ type = Null; }
    virtual guint32 lengthData(){ return 0; }
    virtual void writeNext(void *buf,guint32 len);
    void stop(){}
};

class GStreamerSourceBase : public Source
{
public:
    GStreamerSourceBase(std::string const& _name):Source(_name){}
};

class GStreamerSampleSource :
    public GStreamerSourceBase,
    public GStreamer::GStreamerSrc
{
public:
    GStreamerSampleSource(GStreamer::GStreamer *gst,std::string const& _name,guint32 maxSampleSize);
    virtual guint32 lengthData();
    virtual guint32 getTTL();
    virtual guint32 getTau();
    virtual size_t getAlign();
    virtual guint32 sizeT();
    virtual uuids::uuid getUuid();
    void writeToFile(Request *,string rpath);
    friend class GStreamer::GStreamerStreamSource;
    friend class GStreamer::GStreamer;
protected:
    void writeNext(void *mem,guint32 len);
    void writeNextSample(GObject *sample);
private:
    guint32 m_maxSampleSize;
    json::value &meta;
};

class GStreamerOffsetSource :
    public GStreamerSourceBase,
    public GStreamer::GStreamerStreamSource
{
public:
    GStreamerOffsetSource(GStreamer::GStreamer *gst,std::string const& _name);
    virtual guint32 lengthData();
    virtual guint32 getTTL();
    virtual guint32 getTau();
    virtual size_t getAlign();
    virtual guint32 sizeT();
    virtual uuids::uuid getUuid();
    friend class GStreamer::GStreamerStreamSource;
    friend class GStreamer::GStreamer;
    void writeNext(void *mem,guint32 len);
protected:
    virtual void writeNextSample(GObject *sample);
};

}

#endif // SOURCE_H
