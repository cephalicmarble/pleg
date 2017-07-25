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
#include "jsonconfig.h"
#include "application.h"
#include <math.h>

namespace Pleg {

namespace Sources {

/**
 * @brief Source::~Source : flush our buffers
 */
Source::~Source()
{
    const Relevance rel(std::move(Relevance::fromSource(this)));
    Cache(CPS_call_void(Buffers::clearRelevantBuffers,&rel));
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
    writeNextHere(const_cast<void*>(buf->data<void>()),buf->length());
    buf->getRelevance()->setSource(this);
    buf->TTL(getTTL());
    Cache(CPS_call_void(Buffers::addSourceBuffer,const_cast<const Buffers::SourceBuffer*>(buf)));
}

void Source::writeToFile(Request *req,string rpath)
{
    Files::files.add(req->getRelevanceRef(),rpath);
}

void Source::getStatus(json::value *status)
{
    status->get_object().insert({"actions","chart,record"});
}

void Source::report(json::value *obj,ReportType type)const
{
    obj->get_object().insert({"ticks",tick});
    if(type & WorkObject::ReportType::Memory){
        const Buffers::heap_t *heap(Allocator(CPS_call_void(Buffers::getHeap,dynamic_cast<const Pleg::Sources::Source*>(this))));
        if(!heap)
            return;
        obj->get_object().insert({"allocated",heap->allocated * heap->size});
        obj->get_object().insert({"reserved",heap->max});
    }
}

/**
 * @brief MockSource::MockSource : default constructor
 */
MockSource::MockSource() : Base("mock"),timer(drumlin::io_service)
{
    type = Source_Mock;
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
    timer.async_wait(boost::bind(&MockSource::timedOut,this,asio::placeholders::error));
}

void MockSource::stop()
{
    timer.cancel();
}

/**
 * @brief MockSource::read : writes the alphabet, with null terminating byte.
 * @param buf void*
 * @param len quint32
 * @return quint32 bytesWritten
 */
void MockSource::writeNext(void *buf,guint32)
{
    (*(guint64*)buf) = round(100.0*(1.0*rand()/RAND_MAX));
}

GStreamerSampleSource::GStreamerSampleSource(GStreamer::GStreamer *gst,std::string _name,guint32 maxSampleSize)
    :GStreamerSourceBase(_name),src(gst,_name),m_maxSampleSize(maxSampleSize)
{
    type = Source_Gstreamer;
    Config::JsonConfig gst_config(Config::load(Config::gstreamer_config_file));
    string key("/pipes/");
    meta.reset(new json::value(gst_config.at(key+src.getName())));
}

void GStreamerSampleSource::writeNextSample(GstSample *sample)
{
    if(!sample)
        return;
#if GST_VERSION_MAJOR == 0
    writeNext(GST_BUFFER_DATA(sample),GST_BUFFER_SIZE(sample));
#else
    GstBuffer *buffer = gst_sample_get_buffer(GST_SAMPLE(sample));
    GstMapInfo info;
    if(!buffer)
        return;
    if(gst_buffer_map(buffer,&info,GST_MAP_READ)){
        writeNext(info.data,info.size);
        gst_buffer_unmap(buffer,&info);
    }
#endif
}

guint32 GStreamerSampleSource::lengthData()
{
    return src.len;
}

guint32 GStreamerSampleSource::getTTL()
{
    return 500;
}

guint32 GStreamerSampleSource::getTau()
{
    return floor(1000/src.fps);
}

size_t GStreamerSampleSource::getAlign()
{
    return alignof(guint32);
}

guint32 GStreamerSampleSource::sizeT()
{
    return m_maxSampleSize;
}

uuids::uuid GStreamerSampleSource::getUuid()
{
    return Pleg::string_gen("0f0f0f0f0f0f0f0f0101010101010101");
}

void GStreamerSampleSource::writeNext(void *mem,guint32 len)
{
    Buffers::SourceBuffer *buf(new Buffers::SourceBuffer(this));
    if(!buf->isValid()){
        delete buf;
        return;
    }
    buf->m_len = std::min(len,buf->m_len);
    memcpy(buf->m_data,mem,buf->m_len);
    buf->getRelevance()->setSource(this);
    buf->TTL(getTTL());
    Cache(CPS_call_void(Buffers::addSourceBuffer,const_cast<const Buffers::SourceBuffer*>(buf)));
}

void GStreamerSampleSource::writeToFile(Request *req,string rpath)
{
    req->getRelevance()->arguments.insert({"filepath",rpath});
    Relevance rel(req->getRelevanceRef());
    threads_type threads(app->findThread("gstreamer",ThreadType_gstreamer));
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
        make_pod_event(EventType::GstStreamFile,"openFileSink",req)->send(thread);
        break;
    }
    rel.setSourceName(rel.getSourceName()+".file");
    Files::files.add(rel,rpath);
    if(already)
        make_event(EventType::GstStreamFile,"open")->send(req->getThread());
}

void GStreamerSampleSource::getStatus(json::value *status)
{
    status->get_object().insert({"actions","view,record"});
}

GStreamerOffsetSource::GStreamerOffsetSource(GStreamer::GStreamer *gst,std::string _name)
    :GStreamerSourceBase(_name),src(gst,_name)
{
    type = Source_Offset;
}

void GStreamerOffsetSource::writeNextSample(GstSample *sample)
{
    guint64 offset(src.getTick());
    writeNext(&offset,sizeof(offset));
    src.writeFrame(sample);
}

guint32 GStreamerOffsetSource::lengthData()
{
    return sizeof(guint64);
}

guint32 GStreamerOffsetSource::getTTL()
{
    return 500;
}

guint32 GStreamerOffsetSource::getTau()
{
    return floor(1000/src.fps);
}

size_t GStreamerOffsetSource::getAlign()
{
    return alignof(guint64);
}

guint32 GStreamerOffsetSource::sizeT()
{
    return sizeof(guint64);
}

uuids::uuid GStreamerOffsetSource::getUuid()
{
    return Pleg::string_gen("0f0f0f0f0f0f0f0f0202020202020202");
}

void GStreamerOffsetSource::writeNext(void *mem,guint32 len)
{
    Buffers::SourceBuffer *buf(new Buffers::SourceBuffer(this));
    if(!buf->isValid()){
        delete buf;
        return;
    }
    buf->m_len = std::min(len,buf->m_len);
    memcpy(buf->m_data,mem,buf->m_len);
    buf->getRelevance()->setSource(this);
    buf->TTL(getTTL());
    Cache(CPS_call_void(Buffers::addSourceBuffer,const_cast<const Buffers::SourceBuffer*>(buf)));
}

void GStreamerOffsetSource::getStatus(json::value *status)
{
    status->get_object().insert({"actions","chart,record"});
}

sources_type sources;

void getStatus(json::value *status)
{
    std::lock_guard<std::recursive_mutex> l(sources.mutex);
    json::value array_sources(json::empty_array);
    int index = 0;
    for(Sources::sources_type::value_type const& source : sources){
        json::value obj{ {
            { "name", source.first },
            { "index", index++ },
            { "type", metaEnum<Source::Type>().toString(source.second->type) },
        } };
        source.second->getStatus(&obj);
        array_sources.get_array().push_back(obj);
    }
    GStreamer::GStreamer *gst(GStreamer::GStreamer::getInstance());
    if(gst){
        std::lock_guard<std::recursive_mutex> lgst(const_cast<std::recursive_mutex&>(gst->getJobs().mutex));
        for(ThreadWorker::jobs_type::value_type const& source : gst->getJobs()){
            Sources::GStreamerSourceBase *gsrc(const_cast<Sources::GStreamerSourceBase*>(dynamic_cast<Sources::GStreamerSourceBase*>(source.second)));
            json::value obj{ {
                { "name", source.first },
                { "index", index++ },
                { "type", metaEnum<Source::Type>().toString(gsrc->type) },
            } };
            gsrc->getStatus(&obj);
            array_sources.get_array().push_back(obj);
        }
    }
    status->get_object().insert({"sources",array_sources});
}

} // namespace Source

} // namespace Pleg
