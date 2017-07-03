#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include <boost/uuid/string_generator.hpp>
using namespace boost;
#include "source.h"
#include "buffer.h"
#include "gstreamer.h"
#include "request.h"
#include "files.h"
#include <math.h>

namespace Sources {

/**
 * @brief Source::~Source : flush our buffers
 */
Source::~Source()
{
    const Relevance rel(std::move(Relevance::fromSource(this)));
    Buffers::Cache(CPS_call_void(Buffers::clearRelevantBuffers,&rel));
}

/**
 * @brief Source::writeNext : prepares a new buffer read from this source
 */
void Source::writeNext()
{
    Buffers::SourceBuffer *buf(new Buffers::SourceBuffer(this));
    if(!buf->isValid()){
        delete buf;
        return;
    }
    writeNext(const_cast<void*>(buf->data<void>()),buf->length());
    buf->getRelevance()->setSource(this);
    buf->TTL(getTTL());
    Buffers::Cache(CPS_call_void(Buffers::addSourceBuffer,const_cast<const Buffers::SourceBuffer*>(buf)));
}

void Source::report(json::value *obj,ReportType type)const
{
    obj->get_object().insert({"ticks",tick});
    if(type & WorkObject::ReportType::Memory){
        obj->get_object().insert({"allocated",memory});
    }
}

void Source::writeToFile(Request *req,tring rpath)
{
    Files::files.add(req->getRelevanceRef(),rpath);
}

/**
 * @brief MockSource::MockSource : default constructor
 */
MockSource::MockSource() : Base("mock")
{
    type = Mock;
}

/**
 * @brief MockSource::timedOut : nextTick
 */
void MockSource::timedOut(const system::error_code& e)
{
    Source::writeNext();
    if(!e){
        start();
    }
}

/**
 * @brief MockSource::start
 */
void MockSource::start()
{
    timer.expires_from_now(posix_time::seconds(1));
    timer.async_wait(boost::bind(timedOut,asio::placeholders::error));
}

void MockSource::stop()
{
    timer.expires_from_now(posix_time::microsec_clock::universal_time());
}

/**
 * @brief MockSource::read : writes the alphabet, with null terminating byte.
 * @param buf void*
 * @param len quint32
 * @return quint32 bytesWritten
 */
void MockSource::writeNext(void *buf,quint32 len)
{
    (*(guint64*)buf) = round(100.0*(1.0*rand()/RAND_MAX));
}

GStreamerSampleSource::GStreamerSampleSource(GStreamer::GStreamer *gst,std::string const& _name,guint32 maxSampleSize)
    :GStreamerSourceBase(_name),GStreamer::GStreamerSrc(gst),m_maxSampleSize(maxSampleSize)
{
    LOCK
    type = Gstreamer;
    Config::JsonConfig gst_config("./gstreamer.json");
    string key("/pipes/");
    meta = gst_config.at(key+getName());
}

void GStreamerSampleSource::writeNextSample(GObject *sample)
{
    if(!sample)
        return;
    GstBuffer *buffer = gst_sample_get_buffer(GST_SAMPLE(sample));
    GstMapInfo info;
    if(!buffer)
        return;
    if(gst_buffer_map(buffer,&info,GST_MAP_READ)){
        writeNext(info.data,info.size);
        gst_buffer_unmap(buffer,&info);
    }
}

guint32 GStreamerSampleSource::lengthData()
{
    return len;
}

guint32 GStreamerSampleSource::getTTL()
{
    return 500;
}

guint32 GStreamerSampleSource::getTau()
{
    return floor(1000/src->fps);
}

size_t GStreamerSampleSource::getAlign()
{
    guint32 tmp[sizeT()];
    return alignof(tmp);
}

guint32 GStreamerSampleSource::sizeT()
{
    return m_maxSampleSize;
}

uuids::uuid GStreamerSampleSource::getUuid()
{
    return Pleg::string_gen("gstreamer-sample");
}

void GStreamerSampleSource::writeNext(void *mem,guint32 len)
{
    Buffers::SourceBuffer *buf(new Buffers::SourceBuffer(this));
    if(!buf->isValid()){
        delete buf;
        return;
    }
    buf->m_len = qMin(len,buf->m_len);
    memcpy(buf->m_data,mem,buf->m_len);
    buf->getRelevance()->setSource(this);
    buf->TTL(getTTL());
    Buffers::Cache(CPS_call_void(Buffers::addSourceBuffer,const_cast<const Buffers::SourceBuffer*>(buf)));
}

void GStreamerSampleSource::writeToFile(Request *req,string rpath)
{
    req->getRelevance()->arguments.insert({"filepath",rpath});
    Relevance rel(req->getRelevanceRef());
    threads_type threads(app->findThread("gstreamer",ThreadWorker::ThreadType::gstreamer));
    if(0==std::distance(threads.begin(),threads.end())){
        return;
    }
    bool already = false;
    for(threads_type::iterator::value_type const& thread : threads){
        GStreamer::GStreamer *gst(dynamic_cast<GStreamer::GStreamer*>(thread->getWorker()));
        if(!gst)
            return;
        if(gst->getJobs().end() != std::find_if(gst->getJobs().begin(),gst->getJobs().end(),[rel](GStreamer::GStreamer::jobs_type::value_type const& job){
            return job.first == (rel.getSourceName()+".file");
        })) { already = true; break; }
        make_pod_event(Event::Type::GstStreamFile,"openFileSink",req)->send(thread);
        break;
    }
    rel.setSourceName(rel.getSourceName()+".file");
    Files::files.add(rel,rpath);
    if(already)
        make_event(Event::Type::GstStreamFile,"open")->send(req->getThread());
}

GStreamerOffsetSource::GStreamerOffsetSource(GStreamer::GStreamer *gst,std::string const& _name)
    :GStreamerSourceBase(_name),GStreamer::GStreamerStreamSource(gst)
{
    LOCK
    type = Offset;
}

void GStreamerOffsetSource::writeNextSample(GObject *sample)
{
    quint64 offset(src->getTick());
    writeNext(&offset,sizeof(offset));
    GStreamer::GStreamerStreamSource::writeFrame(sample);
}

quint32 GStreamerOffsetSource::lengthData()
{
    return sizeof(quint64);
}

quint32 GStreamerOffsetSource::getTTL()
{
    return 500;
}

quint32 GStreamerOffsetSource::getTau()
{
    return floor(1000/src->getSrc()->fps);
}

size_t GStreamerOffsetSource::getAlign()
{
    return alignof(quint64);
}

quint32 GStreamerOffsetSource::sizeT()
{
    return sizeof(quint64);
}

uuids::uuid GStreamerOffsetSource::getUuid()
{
    return Pleg::string_gen("gstreamer-offset");
}

void GStreamerOffsetSource::writeNext(void *mem,quint32 len)
{
    Buffers::SourceBuffer *buf(new Buffers::SourceBuffer(this));
    if(!buf->isValid()){
        delete buf;
        return;
    }
    buf->m_len = qMin(len,buf->m_len);
    memcpy(buf->m_data,mem,buf->m_len);
    buf->getRelevance()->setSource(this);
    buf->TTL(getTTL());
    Buffers::Cache(CPS_call_void(Buffers::addSourceBuffer,const_cast<const Buffers::SourceBuffer*>(buf)));
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
            { "type", metaEnum<Source::Type>().toString(source.second->type) },
        } };
        if(dynamic_cast<GStreamerSampleSource*>(source.second)){
            obj.get_object().insert({"actions","view,record"});
        }else{
            obj.get_object().insert({"actions","chart,record"});
        }
        source.second->report(&obj,Source::ReportType::All);
        array_sources.get_array().push_back(obj);
    }
    status->get_object().insert({"sources",array_sources});
}

}
