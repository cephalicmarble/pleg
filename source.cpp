#include <qs.h>
using namespace QS;
#include <tao/json.hh>
using namespace tao;
#include "source.h"
#include <QDebug>
#include "buffer.h"
#ifdef GSTREAMER
#include "gstreamer.h"
#endif
#include "request.h"
#include "files.h"
#include <math.h>

namespace Sources {

/**
 * @brief QSSource::~QSSource : flush our buffers
 */
QSSource::~QSSource()
{
    const QSRelevance rel(std::move(Relevance::fromSource(this)));
    Buffers::Cache(CPS_call_void(Buffers::clearRelevantBuffers,&rel));
}

/**
 * @brief QSSource::writeNext : prepares a new buffer read from this source
 */
void QSSource::writeNext()
{
    Buffers::QSSourceBuffer *buf(new Buffers::QSSourceBuffer(this));
    if(!buf->isValid()){
        delete buf;
        return;
    }
    writeNext(const_cast<void*>(buf->data<void>()),buf->length());
    buf->getRelevance()->setSource(this);
    buf->TTL(getTTL());
    Buffers::Cache(CPS_call_void(Buffers::addSourceBuffer,const_cast<const Buffers::QSSourceBuffer*>(buf)));
}

void QSSource::report(json::value *obj,ReportType type)const
{
    obj->get_object().insert({"ticks",tick});
    if(type & QSWorkObject::ReportType::Memory){
        obj->get_object().insert({"allocated",memory});
    }
}

void QSSource::writeToFile(QSRequest *req,QString rpath)
{
    Files::files.add(req->getRelevanceRef(),rpath);
}

/**
 * @brief QSMockSource::QSMockSource : default constructor
 */
QSMockSource::QSMockSource() : Base("mock")
{
    type = Mock;
    timer.setInterval(1000);
    connect(&timer,SIGNAL(timeout()),this,SLOT(timedOut()));
}

/**
 * @brief QSMockSource::timedOut : nextTick
 */
void QSMockSource::timedOut()
{
    if(!timer.isActive())return;
    QSSource::writeNext();
}

/**
 * @brief QSMockSource::start
 */
void QSMockSource::start()
{
    timer.start();
}

/**
 * @brief QSMockSource::read : writes the alphabet, with null terminating byte.
 * @param buf void*
 * @param len quint32
 * @return quint32 bytesWritten
 */
void QSMockSource::writeNext(void *buf,quint32 len)
{
    (*(quint64*)buf) = std::round(100.0*(1.0*rand()/RAND_MAX));
}

#if defined(GSTREAMER)

quint32 QSGStreamerSource::lengthData()
{
    return src->len;
}

quint32 QSGStreamerSource::getTTL()
{
    return 500;
}

quint32 QSGStreamerSource::getTau()
{
    return floor(1000/src->fps);
}

size_t QSGStreamerSource::getAlign()
{
    quint32 tmp[sizeT()];
    return alignof(tmp);
}

quint32 QSGStreamerSource::sizeT()
{
    return 4*src->width*src->height;
}

QUuid QSGStreamerSource::getUuid()
{
    return QUuid("gstreamer");
}

#if defined(QTGSTREAMER)
void QSGStreamerSource::writeNext(QGst::SamplePtr const& sample)
{
    QGst::BufferPtr buffer(sample->buffer());
    Buffers::QSSourceBuffer *buf(new Buffers::QSSourceBuffer(this));
    buf->m_len = buffer->size();
    buffer->extract(0,buf->m_data,buf->m_len);
    buf->getRelevance()->setSource(this);
    buf->TTL(getTTL());
    Buffers::Cache(CPS_call_void(Buffers::addSourceBuffer,const_cast<const Buffers::QSSourceBuffer*>(buf)));
}
#elif defined(GSTREAMER)
void QSGStreamerSource::writeNext(void *mem,quint32 len)
{
    Buffers::QSSourceBuffer *buf(new Buffers::QSSourceBuffer(this));
    if(!buf->isValid()){
        delete buf;
        return;
    }
    buf->m_len = qMin(len,buf->m_len);
    memcpy(buf->m_data,mem,buf->m_len);
    buf->getRelevance()->setSource(this);
    buf->TTL(getTTL());
    Buffers::Cache(CPS_call_void(Buffers::addSourceBuffer,const_cast<const Buffers::QSSourceBuffer*>(buf)));
}
#endif

void QSGStreamerSource::writeToFile(QSRequest *req,QString rpath)
{
    req->getRelevance()->arguments.insert({"filepath",rpath});
    QSRelevance rel(req->getRelevanceRef());
    threads_type threads(app->findThread("gstreamer",QSThreadWorker::QSThreadType::gstreamer));
    if(0==std::distance(threads.begin(),threads.end())){
        return;
    }
    bool already = false;
    for(threads_type::iterator::value_type const& thread : threads){
        GStreamer::QSGStreamer *gst(dynamic_cast<GStreamer::QSGStreamer*>(thread->getWorker()));
        if(!gst)
            return;
        if(gst->getJobs().end() != std::find_if(gst->getJobs().begin(),gst->getJobs().end(),[rel](GStreamer::QSGStreamer::jobs_type::value_type const& job){
            return job.first == (rel.getSourceName()+".file");
        })) { already = true; break; }
        make_pod_event(QSEvent::Type::GstStreamFile,"openFileSink",req)->send(thread);
        break;
    }
    rel.setSourceName(rel.getSourceName()+".file");
    Files::files.add(rel,rpath);
    if(already)
        make_event(QSEvent::Type::GstStreamFile,"open")->send(req->getQSThread());
}

quint32 QSGStreamerOffsetSource::lengthData()
{
    return sizeof(quint64);
}

quint32 QSGStreamerOffsetSource::getTTL()
{
    return 500;
}

quint32 QSGStreamerOffsetSource::getTau()
{
    return floor(1000/src->getSrc()->fps);
}

size_t QSGStreamerOffsetSource::getAlign()
{
    return alignof(quint64);
}

quint32 QSGStreamerOffsetSource::sizeT()
{
    return sizeof(quint64);
}

QUuid QSGStreamerOffsetSource::getUuid()
{
    return QUuid("gstreamer");
}

void QSGStreamerOffsetSource::writeNext(void *mem,quint32 len)
{
    Buffers::QSSourceBuffer *buf(new Buffers::QSSourceBuffer(this));
    if(!buf->isValid()){
        delete buf;
        return;
    }
    buf->m_len = qMin(len,buf->m_len);
    memcpy(buf->m_data,mem,buf->m_len);
    buf->getRelevance()->setSource(this);
    buf->TTL(getTTL());
    Buffers::Cache(CPS_call_void(Buffers::addSourceBuffer,const_cast<const Buffers::QSSourceBuffer*>(buf)));
}

#endif

sources_type sources;

void getStatus(json::value *status)
{
    QMutexLocker ml(&Sources::sources.mutex);
    json::value array_sources(json::empty_array);
    int index = 0;
    for(Sources::sources_type::value_type const& source : Sources::sources){
        json::value obj{ {
            { "name", source.first },
            { "index", index++ },
            { "type", QMetaEnum::fromType<QSSource::Type>().valueToKey(source.second->type) },
        } };
        if(dynamic_cast<QSGStreamerSource*>(source.second)){
            obj.get_object().insert({"actions","view,record"});
        }else{
            obj.get_object().insert({"actions","chart,record"});
        }
        source.second->report(&obj,QSSource::ReportType::All);
        array_sources.get_array().push_back(obj);
    }
    status->get_object().insert({"sources",array_sources});
}

}
