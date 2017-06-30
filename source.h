#ifndef QSSOURCE_H
#define QSSOURCE_H

#include <pleg.h>
using namespace Pleg;
#include <string>
#include "boost/date_time/posix_time/posix_time.hpp"
#include <map>
#include "mutexcall.h"
#include "buffer.h"
#include "registry.h"
#include "tao_forward.h"
using namespace tao;

class QSRequest;
class QSBluetoothLEDevice;
#if defined(GSTREAMER)
#include "gstreamer.h"
#endif
#if defined(QTGSTREAMER)
#include "qtgstreamer.h"
#endif

namespace Sources {

/**
 * @brief The QSSource class : represents an abstract data source
 */
class QSSource :
    public QObject,
    public QSWorkObject
{
    Q_OBJECT
    quint64 tick = 0;
    size_t memory;
    std::string name;
public:
    enum Type {
        HeartRate,
        BatteryLevel,
        Mock,
        Null,
        Video,
        Offset
    };
    Q_ENUM(Type)
    Type type;

    bool deleting = false;
    QSSource(std::string const& _name):QObject(),name(_name){}
    QSSource():QObject(),name("source"){}
    virtual ~QSSource();
    virtual void stop(){}
    virtual quint64 nextTick(){
        return ++tick;
    }
    std::string const& getName()const { return name; }
    void setMemory(size_t _memory){ memory = _memory; }

    /**
     * @brief QSSource::writeNextValue
     * @param bytes void*
     * @param len quint32
     */
    virtual void writeNext(void *bytes, quint32 len){}
    virtual void writeNext();
    virtual quint32 lengthData(){ return 0; }
    virtual quint32 getTTL(){ return 2000; }
    virtual quint32 getTau(){ return 1000; }
    virtual size_t getAlign(){ return 1; }
    virtual quint32 sizeT(){ return 0; }

    virtual bool isDestructing(){return false;}
    virtual QUuid getUuid(){ return QUuid(); }

    operator const char*()const{ return name.c_str(); }
    virtual void report(json::value *obj,ReportType type)const;

    virtual void writeToFile(QSRequest *,QString rpath);
};

/**
 * @brief The QSSource class : represents an abstract data source
 */
template <size_t _len = 512>
class QSBufferedSource :
    public QSSource
{
protected:
    quint8 data[_len];
    const quint32 maxlen = _len;
    quint32 len;
    QUuid uuid;
public:
    /**
     * @brief QSBufferedSource : default constructor
     */
    QSBufferedSource(std::string const& name):QSSource(name){}

    quint8 *_data(){ return data; }
    /**
     * @brief QSBufferedSource::writeNext : buffer next sample
     */
    virtual void writeNextValue(const QByteArray &value, const QUuid &_uuid)
    {
        uuid = _uuid;
        memcpy((void*)data,(void*)value.data(),len = qMin((quint32)value.length(),maxlen));
    }

    /**
     * @brief QSBufferedSource::lengthData : return the buffered sample length
     * @return quint32
     */
    virtual quint32 lengthData()
    {
        return len;
    }

    virtual quint32 sizeT()
    {
        return maxlen;
    }

    /**
     * @brief read : from the input buffer
     * @param buf void*
     * @param len quint32
     * @return quint32
     */
    virtual quint32 read(void *buf,quint32 len)
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
    virtual QUuid getUuid(){ return uuid; }

    virtual size_t getAlign(){ return alignof(data); }

    /**
     * @brief operator const char *
     */
    virtual operator const char*(){ return "buffered"; }
};

template <class T>
class QSTypedSource :
    public QSSource
{
protected:
    T t;
    QUuid uuid;
public:
    /**
     * @brief QSTypedSource : default constructor
     */
    QSTypedSource(std::string const& name):QSSource(name){}
    /**
     * @brief getData
     * @return T* where T=the buffered data type (see QSHeartRateSource)
     */
    T*getData(){ return &t; }
    virtual operator const char*(){ return "typed"; }
    /**
     * @brief QSTypedSource<T>::lengthData : return the buffered sample length
     * @return quint32
     */
    virtual quint32 lengthData()
    {
        return sizeT();
    }

    virtual quint32 sizeT()
    {
        return sizeof(T);
    }

    /**
     * @brief QSTypedSource<T>::writeNextValue
     * @param bytes void*
     * @param len quint32
     */
    void writeNext(void *bytes, quint32 len)
    {
        Buffers::QSSourceBuffer *buf(new Buffers::QSSourceBuffer(this));
        memcpy(getData()->data,bytes,len);
        buf->getRelevance()->setSource(this);
        buf->TTL(getTTL());
        Buffers::Cache(CPS_call_void(Buffers::addSourceBuffer,const_cast<const Buffers::QSSourceBuffer*>(buf)));
    }

    /**
     * @brief getUuid
     * @return QUuid
     */
    virtual QUuid getUuid(){ return uuid; }

    virtual size_t getAlign(){ return alignof(T); }
};

class QSSourceRegistry :
    public QSRegistry<QSSource>
{
public:
    /**
     * @brief add : TBC from QSServer only, inserts a new QString QSSource* mapping
     * @param str QString
     * @param src QSSource*
     */
    void add(const std::string &str,QSSource *src)
    {
        QMutexLocker ml(&mutex);
        src->setMemory(Allocator(CPS_call_void(Buffers::registerSource,src)));
        QSRegistry<QSSource>::add(str,src);
    }

    /**
     * @brief remove : remove a QString QSSource* mapping, delete the QSSource
     * @param str
     */
    void remove(const std::string &str)
    {
        QMutexLocker ml(&mutex);
        Allocator(CPS_call_void(Buffers::unregisterSource,map->at(str)));
        QSRegistry<QSSource>::remove(str);
    }

    /**
     * @brief removeAll : remove a QString QSSource* mapping, delete the QSSource
     * @param str
     */
    void removeAll()
    {
        QMutexLocker ml(&mutex);
        Allocator(CPS_call_void(Buffers::unregisterSources,0));
        QSRegistry<QSSource>::removeAll();
    }
    friend void Sources::getStatus(json::value*);
};

/**
 * @brief sources_type : map of named sources
 */
typedef QSSourceRegistry sources_type;
extern sources_type sources;

void getStatus(json::value *status);

/**
 * @brief The QSMockSource class : enumerates the alphabet into a buffer
 */
class QSMockSource :
    public QSBufferedSource<32>
{
    Q_OBJECT

    typedef QSBufferedSource<32> Base;
    QTimer timer;
public:
    QSMockSource();
    void start();
    virtual void writeNext(void *buf,quint32 len);
    virtual QUuid getUuid(){ return QUuid("1"); }
    void stop(){
        timer.stop();
    }
public slots:
    void timedOut();
};

/**
 * @brief The QSNullSource class : does nothing.
 */
class QSNullSource : public QSSource
{
public:
    QSNullSource():QSSource("null"){ type = Null; }
    virtual quint32 lengthData(){ return 0; }
    virtual void writeNext(void *buf,quint32 len);
    void stop(){}
};

#if defined(GSTREAMER) || defined(QTGSTREAMER)

class QSGStreamerSourceBase : public QSSource
{
public:
    QSGStreamerSourceBase(std::string const& _name):QSSource(_name){}
};

class QSGStreamerSource :
    public QSGStreamerSourceBase
{
    Q_OBJECT

    GStreamer::QSGStreamerSrc *src;
    json::value *meta;
public:
    QSGStreamerSource(GStreamer::QSGStreamer *gst,std::string const& _name);
    ~QSGStreamerSource();
    GStreamer::QSGStreamerSrc *getSrc()const{ return src; }
    virtual quint32 lengthData();
    virtual quint32 getTTL();
    virtual quint32 getTau();
    virtual size_t getAlign();
    virtual quint32 sizeT();
    virtual QUuid getUuid();
    void writeToFile(QSRequest *,QString rpath);
    friend class GStreamer::QSGStreamerVideoStream;
    friend class GStreamer::QSGStreamer;
public slots:
#if defined(QTGSTREAMER)
    virtual void writeNext(QGst::SamplePtr const&);
#elif defined(GSTREAMER)
    void writeNext(void *mem,quint32 len);
    virtual void writeNextSample(GObject *sample);
#endif
    void started();
    void stop();
};

class QSGStreamerOffsetSource :
    public QSGStreamerSourceBase
{
    Q_OBJECT

    GStreamer::QSGStreamerVideoFiler *src;
public:
    QSGStreamerOffsetSource(GStreamer::QSGStreamerVideoFiler *_src,std::string const& _name);
    ~QSGStreamerOffsetSource();
    virtual quint32 lengthData();
    virtual quint32 getTTL();
    virtual quint32 getTau();
    virtual size_t getAlign();
    virtual quint32 sizeT();
    virtual QUuid getUuid();
    QString getFilePath()const{ return src->getFilePath(); }
    friend class GStreamer::QSGStreamerVideoFiler;
    friend class GStreamer::QSGStreamer;
public slots:
    void writeNext(void *mem,quint32 len);
    virtual void writeNextSample(GObject *sample);
    void started();
    void stop();
};

#endif // GSTREAMER

}

#endif // QSSOURCE_H
