#define TAOJSON
#include "gstreamer.h"

#include <functional>
#include <mutex>
using namespace std;
#include <boost/algorithm/string.hpp>
using namespace boost;
#include "drumlin/jsonconfig.h"
#include "drumlin/event.h"
#include "drumlin/cursor.h"
#include "drumlin/mutexcall.h"
using namespace drumlin;
#include "buffer.h"
#include "source.h"
#include "request.h"
#include "pleg.h"
using namespace Pleg;

/* Many thanks for the much-edited cap_gstreamer.cpp example, opencv (cephalicmarble@hotmail.com) */

namespace Pleg {

namespace GStreamer {

void toFraction(double decimal, double &numerator, double &denominator);

std::mutex gst_initializer::gst_initializer_mutex;

#define GST_REASONABLE_WAIT (0.4 * 1000000000.)

// utility functions

/**
 * @brief toFraction
 * @param decimal
 * @param numerator
 * @param denominator
 * Split a floating point value into numerator and denominator
 */
void toFraction(double decimal, double &numerator, double &denominator)
{
    double dummy;
    double whole;
    decimal = modf (decimal, &whole);
    for (denominator = 1; denominator<=100; denominator++){
        if (modf(denominator * decimal, &dummy) < 0.001f)
            break;
    }
    numerator = denominator * decimal;
}

std::mutex bus_mutex;
#define BUSLOCK std::lock_guard<std::mutex> lb(bus_mutex)

gboolean bus_func(GstBus * bus, GstMessage * msg, gpointer user_data)
{
    BUSLOCK;
    GStreamerPipe *gp(dynamic_cast<GStreamerPipe*>(static_cast<Object*>(user_data)));
    if(!gp)
        return false;
    return gp->handleMessage(bus,msg);
}

GStreamerPipe::GStreamerPipe(GStreamer *gst,string _name):WorkObject(),gstreamer(gst),name(_name)
{
    pipeline = 0;
    element = 0;
    caps = 0;
}

std::recursive_mutex mutex;
#define LOCK std::lock_guard<std::recursive_mutex> l(mutex)

GStreamerPipe::~GStreamerPipe()
{
    LOCK;
    Debug() << this << __func__;
    close();
    if(!getGStreamer()->isDeleting())
        make_pod_event(EventType::GstRemoveJob,__func__,getName())->send(getGStreamer()->getThread());
}

void GStreamerPipe::handleMessages()
{
    LOCK;
    BUSLOCK;
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg;
    while(gst_bus_have_pending(bus)) {
        msg = gst_bus_pop(bus);
        //printf("Got %s message\n", GST_MESSAGE_TYPE_NAME(msg));
        handleMessage(bus,msg);
    }
}

gboolean GStreamerPipe::handleMessage(GstBus *, GstMessage *msg)
{
    LOCK;
    GError *err = NULL;
    gchar *debug = NULL;
    GstStreamStatusType tp;
    GstElement * elem = NULL;

    if(gst_is_missing_plugin_message(msg))
    {
        Debug() << "GStreamer: your gstreamer installation is missing a required plugin";
    }
    else
    {
        switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_STATE_CHANGED:
            GstState oldstate, newstate, pendstate;
            gst_message_parse_state_changed(msg, &oldstate, &newstate, &pendstate);
            Debug() << "state changed from " << gst_element_state_get_name(oldstate) << "to" << gst_element_state_get_name(newstate) << "pending" << gst_element_state_get_name(pendstate);
            break;
        case GST_MESSAGE_ERROR:
            gst_message_parse_error(msg, &err, &debug);
            Debug() << "GStreamer Plugin: Embedded video playback halted; module" << gst_element_get_name(GST_MESSAGE_SRC (msg)) << "reported:" << err->message;

            g_error_free(err);
            g_free(debug);

            gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
            break;
        case GST_MESSAGE_EOS:
            Debug() << "reached the end of the stream.";
            break;
        case GST_MESSAGE_STREAM_STATUS:
            gst_message_parse_stream_status(msg,&tp,&elem);
            Debug() << "stream status: elem" << GST_ELEMENT_NAME(elem) << metaEnum<PipelineState>().toString((PipelineState)tp);
            break;
#if GST_VERSION_MAJOR > 0
        case GST_MESSAGE_STREAM_START:
            gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
            break;
#endif
        default:
            Debug() << "unhandled message" << GST_MESSAGE_TYPE_NAME(msg);
            return false;
        }
    }
    return true;
}

/**
 * @brief GStreamerSource::close
 * Closes the pipeline and destroys all instances
 */
void GStreamerPipe::close()
{
    LOCK;
    if(pipeline) {
        gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
        gst_object_unref(GST_OBJECT(pipeline));
        pipeline = NULL;
    }
}

GstState GStreamerPipe::getState()
{
    LOCK;
    GstState current,pending;
    if(!pipeline){
        return GST_STATE_NULL;
    }
    GstStateChangeReturn ret = gst_element_get_state(GST_ELEMENT(pipeline),&current, &pending, 0.4*GST_SECOND);
    if (!ret){
        Debug() << "GStreamer: unable to query pipeline state";
        return GST_STATE_NULL;
    }
    return current;
}

/**
 * @brief GStreamerSource::isPipelinePlaying
 * @return if the pipeline is currently playing.
 */
bool GStreamerPipe::isPlaying()
{
    LOCK;
    GstState current, pending;
    GstClockTime timeout = 5*GST_SECOND;
    if(!pipeline){
        return false;
    }

    GstStateChangeReturn ret = gst_element_get_state(GST_ELEMENT(pipeline),&current, &pending, timeout);
    if (!ret){
        Debug() << "GStreamer: unable to query pipeline state";
        return false;
    }

    return current == GST_STATE_PLAYING;
}

void GStreamerPipe::stateChange(GstState _state = GST_STATE_VOID_PENDING)
{
    LOCK;
    GstState state = GST_STATE_NULL,pending = GST_STATE_NULL;
    GstStateChangeReturn status = gst_element_get_state(pipeline, &state, NULL, GST_REASONABLE_WAIT);
    if(state == _state)return;

    status = gst_element_set_state(GST_ELEMENT(pipeline), _state!=GST_STATE_VOID_PENDING?_state:pending);

    if (status == GST_STATE_CHANGE_ASYNC)
    {
        // wait for status update
        status = gst_element_get_state(pipeline, NULL, NULL, GST_REASONABLE_WAIT);
    }
    if (status == GST_STATE_CHANGE_FAILURE)
    {
        Debug() << gst_element_get_name(pipeline) << ": unable to change state";
        return;
    }
}

/**
 * @brief GStreamerSource::startPipeline
 * Start the pipeline by setting it to the playing state
 */
void GStreamerPipe::start()
{
    LOCK;
    Debug() << this << __func__;

    stateChange(GST_STATE_PLAYING);

    started();

    Debug() << "state now playing";
}


/**
 * @brief GStreamerSource::stopPipeline
 * Stop the pipeline by setting it to NULL
 */
void GStreamerPipe::stop()
{
    LOCK;
    Debug() << this << __func__;
    stateChange();
}

/**
 * @brief GStreamerSource::restartPipeline
 * Restart the pipeline
 */
void GStreamerPipe::restart()
{
    LOCK;
    stop();
    start();
}

/**
 * @brief GStreamerSource::open Open the given file with gstreamer
 * @param filename string pipeline specification
 * @return boolean. Specifies if opening was successful.
 *
 * The 'filename' parameter is one of the following:
 *  - a gstreamer pipeline description:
 *        e.g. videotestsrc ! videoconvert ! appsink
 *        the appsink name should be 'sink0'
 *        the appsrc name should be 'src0'
 */
bool GStreamerPipe::open( std::string filename )
{
    LOCK;
    pipestr = filename;
    Debug() << this << __func__;

    pipeline = gst_parse_launch(filename.c_str(), NULL);

    if(!pipeline){
        Debug() << __func__ << "gst_parse_launch failure";
        return false;
    }

    GstIterator *it = gst_bin_iterate_elements(GST_BIN(pipeline));

    GstElement *_element = NULL;
    gboolean done = false;
    string name;
    GValue value = G_VALUE_INIT;

    while (!done)
    {
#if GST_VERSION_MAJOR > 0
        switch (gst_iterator_next (it, &value))
        {
        case GST_ITERATOR_OK:
            _element = GST_ELEMENT (g_value_get_object (&value));
#else
        switch(gst_iterator_next (it, (gpointer*)&_element))
        {
        case GST_ITERATOR_OK:
#endif
            name = gst_element_get_name(_element);
            if (name.length())
            {
                if(name == "sink0")
                {
                    element = GST_ELEMENT ( gst_object_ref (_element) );
                }
               else if(name == "src0")
                {
                    element = GST_ELEMENT ( gst_object_ref (_element) );
                }
            }
            g_value_unset (&value);

            break;
        case GST_ITERATOR_RESYNC:
            gst_iterator_resync (it);
            break;
        case GST_ITERATOR_ERROR:
        case GST_ITERATOR_DONE:
            done = TRUE;
            break;
        }
    }
    gst_iterator_free (it);

    if (!element)
    {
        Debug() << "GStreamer: cannot find appsink in manual pipeline";
        return false;
    }

    GstFormat format;

    format = GST_FORMAT_DEFAULT;
#if GST_VERSION_MAJOR == 0
    if(!gst_element_query_duration(element, &format, &duration))
#else
    if(!gst_element_query_duration(element, format, &duration))
#endif
    {
        Debug() << "GStreamer: unable to query duration of stream";
        duration = -1;
    }

    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_func, (gpointer)this);
    gst_object_unref (bus);

    return true;
}

void GStreamerPipe::report(json::value *_obj,ReportType)const
{
    LOCK;
    auto &obj(_obj->get_object());
    obj.insert({"pipeline",pipestr});
    if(nullptr == pipeline)return;
    GstState status;
    gst_element_get_state(pipeline, &status, NULL, GST_REASONABLE_WAIT);
    obj.insert({"state",metaEnum<GStreamerPipe::State>().toString((GStreamerPipe::State)status)});
    obj.insert({"frames",frames});
}

GStreamerSrc::GStreamerSrc(GStreamer *gst, string name):Object(0),GStreamerPipe(gst,name)
{
    LOCK;
    duration = -1;
    width = -1;
    height = -1;
    fps = -1;
    callbacks.eos = &eos_callback;
    callbacks.new_preroll = &new_preroll_callback;
#if GST_VERSION_MAJOR == 0
    callbacks.new_buffer = &new_sample_callback;
#else
    callbacks.new_sample = &new_sample_callback;
#endif
}

#define CALLBACK_LOCK static std::mutex s_mutex;std::lock_guard<std::mutex> l(s_mutex)
void eos_callback(GstAppSink *,gpointer user_data)
{
    CALLBACK_LOCK;
    static_cast<GStreamerSrc*>(user_data)->eos();
}
GstFlowReturn new_preroll_callback(GstAppSink *,gpointer user_data)
{
    CALLBACK_LOCK;
    return static_cast<GStreamerSrc*>(user_data)->new_preroll();
}
GstFlowReturn new_sample_callback(GstAppSink *,gpointer user_data)
{
    CALLBACK_LOCK;
    return static_cast<GStreamerSrc*>(user_data)->new_sample();
}

void need_data_callback(GstAppSrc *, guint length, gpointer user_data)
{
    CALLBACK_LOCK;
    static_cast<GStreamerSink*>(user_data)->need_data(length);
}
void enough_data_callback(GstAppSrc *, gpointer user_data)
{
    CALLBACK_LOCK;
    static_cast<GStreamerSink*>(user_data)->enough_data();
}
gboolean seek_data_callback(GstAppSrc *, guint64 offset, gpointer user_data)
{
    CALLBACK_LOCK;
    return static_cast<GStreamerSink*>(user_data)->seek_data(offset);
}

GstFlowReturn GStreamerSrc::new_sample()
{
    frames++;
    return grabFrame()?GST_FLOW_OK:GST_FLOW_ERROR;
}

bool GStreamerSrc::open(std::string _pipeline)
{
    LOCK;
    if(!GStreamerPipe::open(_pipeline)){
        return false;
    }

    gst_app_sink_set_max_buffers (GST_APP_SINK(element), 1);
    gst_app_sink_set_drop (GST_APP_SINK(element), true);
    gst_app_sink_set_emit_signals (GST_APP_SINK(element), 0);
    caps = gst_app_sink_get_caps(GST_APP_SINK(element));
    gst_app_sink_set_callbacks(GST_APP_SINK(element),&callbacks,this,NULL);

    Debug() << __func__ << _pipeline.c_str();

    handleMessages();

    return true;
}

/**
 * @brief GStreamerSource::grabFrame
 * @return bool
 * Grabs a sample from the pipeline, awaiting consumation by retreiveFrame.
 * The pipeline is started if it was not running yet
 */
bool GStreamerSrc::grabFrame()
{
    LOCK;
    if(!pipeline){
        Critical() << "unexpectedly null pipeline";
        return false;
    }

    GstState state;
    GstState pending;
    GstSample *sample = 0;
    gst_element_get_state(pipeline, &state, &pending, GST_REASONABLE_WAIT);
    do{
#if GST_VERSION_MAJOR == 0
        if(state != GST_STATE_PLAYING){
            stateChange(GST_STATE_PLAYING);
        }else{
            if(!sample)
                sample = gst_app_sink_pull_buffer(GST_APP_SINK(element));
        }
#else
        if(state != GST_STATE_PLAYING){
            if(!sample)
               sample = gst_app_sink_pull_preroll(GST_APP_SINK(element));
            stateChange(GST_STATE_PLAYING);
        }else{
            if(!sample)
                sample = gst_app_sink_pull_sample(GST_APP_SINK(element));
        }
#endif
        gst_element_get_state(pipeline, &state, &pending, 0.4 * 1000000000.);
    }while(!sample || state != GST_STATE_PLAYING);

    if(!sample){
        Critical() << "unexpectedly null sample.";
        return false;
    }

    GstBuffer *buffer = 0;

    if(!onlyOnce){
        GstPad* pad = gst_element_get_static_pad(element, "sink");
#if GST_VERSION_MAJOR == 0
        GstCaps* buffer_caps = gst_pad_get_caps(pad);
#else
        GstCaps* buffer_caps = gst_pad_get_current_caps(pad);
#endif
        const GstStructure *structure = gst_caps_get_structure (buffer_caps, 0);

        if (!gst_structure_get_int (structure, "width", &width))
        {
            Debug() << "Cannot query video width";
        }

        if (!gst_structure_get_int (structure, "height", &height))
        {
            Debug() << "Cannot query video height";
        }

        gint num = 0, denom=1;
        if(!gst_structure_get_fraction(structure, "framerate", &num, &denom))
        {
            Debug() << "Cannot query video fps";
        }

        fps = ceil((double)num/(double)denom);

#if GST_VERSION_MAJOR == 0
        len = GST_BUFFER_SIZE(sample);
#else
        buffer = gst_sample_get_buffer(sample);
        if(!buffer){
            gst_sample_unref(sample);
            return false;
        }
        len = gst_buffer_get_size(buffer);

        gst_buffer_unref(buffer);
#endif

        onlyOnce = true;
    }

    getGStreamer()->nextSample(this,sample);

#if GST_VERSION_MAJOR == 0
    gst_buffer_unref(buffer);
#else
    gst_sample_unref(sample);
#endif

    return true;
}

GStreamerSink::GStreamerSink(GStreamer *gst,string name)
    :Object(0),GStreamerPipe(gst,name)
{
    LOCK;
    callbacks.need_data = &need_data_callback;
    callbacks.enough_data = &enough_data_callback;
    callbacks.seek_data = &seek_data_callback;
}

bool GStreamerSink::open( std::string _pipeline )
{
    LOCK;
    if(!GStreamerPipe::open(_pipeline)){
        return false;
    }

    gst_app_src_set_emit_signals (GST_APP_SRC(element), 0);
    gst_app_src_set_callbacks(GST_APP_SRC(element),&callbacks,this,NULL);

    Debug() << __func__ << _pipeline.c_str();

    handleMessages();

    return true;
}

/**
 * @brief GStreamerHttpStream::close
 * ends the pipeline by sending EOS and destroys the pipeline and all
 * elements afterwards
 */
void GStreamerSink::close()
{
    LOCK;
    while(pipeline)
    {
        if (gst_app_src_end_of_stream(GST_APP_SRC(element)) != GST_FLOW_OK)
        {
            Debug() << "Cannot send EOS to GStreamer pipeline";
            break;
        }

        handleMessages();
        //wait for EOS to trickle down the pipeline. This will let all elements finish properly
        GstBus* bus = gst_element_get_bus(pipeline);
        if(!bus)break;
        GstMessage *msg;
        while(true){
            msg = gst_bus_timed_pop_filtered(bus, GST_REASONABLE_WAIT, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
            if(!msg){
                handleMessages();
                continue;
            }
            if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
            {
                Debug() << "Error during VideoWriter finalization";
                break;
            }
            break;
        }
        if(msg != NULL)
        {
            gst_message_unref(msg);
            g_object_unref(G_OBJECT(bus));
        }

        GStreamerPipe::stop();

        break;
    }

    GStreamerPipe::close();
}

/**
 * @brief GStreamerHttpStream::filenameToMimetype
 * @param filename
 * @return mimetype
 * Resturns a container mime type for a given filename by looking at it's extension
 */
const char* GStreamerSink::filenameToMimetype(const char *filename)
{
    LOCK;
    //get extension
    const char *ext = strrchr(filename, '.');
    if(!ext || ext == filename) return NULL;
    ext += 1; //exclude the dot

    // return a container mime based on the given extension.
    // gstreamer's function returns too much possibilities, which is not useful to us

    //return the appropriate mime
    if (strncasecmp(ext,"avi", 3) == 0)
        return (const char*)"video/x-msvideo";

    if (strncasecmp(ext,"mkv", 3) == 0 || strncasecmp(ext,"mk3d",4) == 0  || strncasecmp(ext,"webm",4) == 0 )
        return (const char*)"video/x-matroska";

    if (strncasecmp(ext,"wmv", 3) == 0)
        return (const char*)"video/x-ms-asf";

    if (strncasecmp(ext,"mov", 3) == 0)
        return (const char*)"video/x-quicktime";

    if (strncasecmp(ext,"ogg", 3) == 0 || strncasecmp(ext,"ogv", 3) == 0)
        return (const char*)"application/ogg";

    if (strncasecmp(ext,"rm", 3) == 0)
        return (const char*)"vnd.rn-realmedia";

    if (strncasecmp(ext,"swf", 3) == 0)
        return (const char*)"application/x-shockwave-flash";

    if (strncasecmp(ext,"mp4", 3) == 0)
        return (const char*)"video/x-quicktime, variant=(string)iso";

    //default to avi
    return (const char*)"video/x-msvideo";
}

void GStreamerSink::stop()
{
    LOCK;
    close();
}

/**
 * @brief GStreamerHttpStream::writeFrame
 * @param image
 * @return
 * Pushes the given frame on the pipeline.
 * The timestamp for the buffer is generated from the framerate set in open
 * and ensures a smooth video
 */
void GStreamerStreamSource::writeFrame( GstSample *sample )
{
    LOCK;
    if(!going)
        return;
//    if(0==frames)
//        return;
//    frames--;
    if(!sample)
        return;

    GstClockTime duration, timestamp;
    GstFlowReturn ret;

    handleMessages();

    duration = ((double)1/fps) * GST_SECOND;
    timestamp = tick * duration;

#if GST_VERSION_MAJOR == 0
    GstBuffer *second = gst_buffer_try_new_and_alloc(GST_BUFFER_SIZE(sample));
    if(!second){
        Critical() << "Could not allocate new GstBuffer";
    }
    memcpy(GST_BUFFER_DATA(second),(guint8*)GST_BUFFER_DATA(sample), GST_BUFFER_SIZE(sample));
    GST_BUFFER_DURATION(second) = duration;
    GST_BUFFER_TIMESTAMP(second) = timestamp;
#else
    //gst_app_src_push_buffer takes ownership of the buffer, so we need to supply it a copy
    GstBuffer *buffer = gst_sample_get_buffer(GST_SAMPLE(sample));
    if(!buffer){
        Debug() << "no buffer in sample";
        return;
    }
    GstBuffer *second = gst_buffer_copy_deep(buffer);
    GST_BUFFER_DURATION(second) = duration;
    GST_BUFFER_PTS(second) = timestamp;
    GST_BUFFER_DTS(second) = timestamp;
#endif
    //set the current number in the frame
    GST_BUFFER_OFFSET(second) =  tick++;
    ret = GST_FLOW_ERROR;
    if(GST_IS_BUFFER(second)){
        ret = gst_app_src_push_buffer(GST_APP_SRC(element), second);
    }else{
        Debug() << "Couldn't copy buffer";
    }
    if (ret != GST_FLOW_OK) {
        Debug() << "Error pushing buffer to GStreamer pipeline";
        return;
    }
}

GStreamerStreamSource::GStreamerStreamSource(GStreamer *gst,string name):GStreamerSink(gst,name),tick(0)
{
    LOCK;
}

void GStreamerStreamSource::nextSample(GStreamerSrc *,GstSample *sample)
{
    writeFrame(sample);
}

void GStreamerStreamSource::args(Relevance::arguments_type const& args)
{
    LOCK;
    copy(args.begin(),args.end(),inserter(m_args,m_args.begin()));
}

bool GStreamerStreamSource::open(std::string filename)
{
    LOCK;
    string pipeline = filename;
    for(auto & arg : m_args){
        string needle(arg.first);
        auto index(pipeline.find(needle));
        if(index==string::npos)
            continue;
        pipeline = pipeline.replace(index,needle.length(),arg.second);
    }
    return GStreamerSink::open(pipeline.c_str());
}

void GStreamerStreamSource::report(json::value *_obj,ReportType type)const
{
    LOCK;
    GStreamerSink::report(_obj,type);
    json::value::object_t &obj(_obj->get_object());
    if(m_args.end()!=m_args.find("ip")){
        obj.insert({"udp",m_args.at("ip")+":"+m_args.at("port")});
    }else{
        obj.insert({"filepath",m_args.at("filepath")});
    }
    obj.insert({"ticks",tick});
}

void GStreamerStreamSource::need_data(guint32 length)
{
    frames = length;
}
void GStreamerStreamSource::enough_data()
{
    frames = 0;
}

void GStreamerStreamSource::start()
{
    LOCK;
    GstPad *source_pad = gst_element_get_static_pad(GST_ELEMENT(element),"sink");
    if(!source_pad){
        handleMessages();
        return;
    }
#if GST_VERSION_MAJOR == 0
    GstCaps *caps = gst_pad_get_caps(source_pad);
#else
    GstCaps *caps = gst_pad_get_current_caps(source_pad);
#endif
    gst_app_src_set_caps(GST_APP_SRC(element),caps);

    going = true;
    frames = 1;

    GStreamerPipe::start();

    return;
}

void GStreamerStreamSource::stop()
{
    going = false;
}

void GStreamer::nextSample(GStreamerSrc *that,GstSample *sample)
{
    LOCK;
    for(auto & connection : connections){
        if(connection.first == that){
            connection.second->writeNextSample(sample);
        }
    }
    boost::this_thread::yield();
}

/**
 * @brief GStreamer::shutdown : stop things
 */
void GStreamer::shutdown()
{
    LOCK;
    Debug() << this << __func__;
    connections.clear();
    m_jobs.removeAll();
    signalTermination();
}

/**
 * @brief GStreamer::gstreamerEventFilter
 * @param obj QObject*
 * @param event Event*
 */
bool GStreamer::event(Event *pevent)
{
    quietDebug() << this << __func__ << metaEnum<EventType>().toString((EventType)pevent->type());
    if((guint32)pevent->type() < (guint32)Event_first
            || (guint32)pevent->type() > (guint32)Event_last){
        return false;
    }
    do{
        switch(pevent->type()){
        case EventType::GstConnectDevices:
        {
            Config::JsonConfig gst_config(Config::load(Config::gstreamer_config_file));
            json::value pipes(gst_config.at("/pipes"));
            for(auto it : pipes.get_object()){
                addPipeline(it.second[string("pipeline")].get_string(),it.first,it.second[string("maxSampleSize")].get_unsigned());
            }
            break;
        }
        case EventType::GstConnectPipeline:
        {
            Config::JsonConfig gst_config(Config::load(Config::gstreamer_config_file));
            Request *request;
            pod_event_cast(pevent,&request);
            Relevance const& rel(request->getRelevanceRef());
            json::value &pipe(gst_config.at(string("/pipes/")+rel.arguments.at("name")));
            addPipeline(pipe[string("pipeline")].get_string(),rel.arguments.at("name"),pipe[string("maxSampleSize")].get_unsigned());
            make_event(pevent->type(),"open")->send(request->getThread());
            break;
        }
        case EventType::GstStreamPort:
        case EventType::GstStreamFile:
        {
            Config::JsonConfig gst_config(Config::load(Config::gstreamer_config_file));
            Request *request;
            pod_event_cast(pevent,&request);
            Relevance const& rel(request->getRelevanceRef());
            string suffix,source;
            if(pevent->type()==GstStreamPort){
                suffix = "tee";
                source = rel.getSourceName();
            }else{
                suffix = "file";
                source = rel.arguments.at("name");
            }
            string pipepath = string("/pipes/")+source+"/"+suffix;
            std::string pipeline = gst_config[pipepath].get_string();
            if(pipeline[0]=='@'){
                pipeline = gst_config.at(pipeline.replace(0,1,1,'/')).get_string();
            }
            addStream(request->getRelevanceRef().arguments,pipeline,source+"."+suffix);
            make_event(pevent->type(),"open")->send(request->getThread());
            break;
        }
        case EventType::GstStreamEnd:
        {
            delete event_cast<Object>(pevent)->getPointer();
            break;
        }
        case EventType::GstRemoveJob:
        {
            if(!isDeleting()){
                LOCK;
                std::lock_guard<std::recursive_mutex> l2(getThread()->m_critical_section);
                string name = pod_event_cast<std::string>(pevent)->getVal().c_str();
                Sources::Source *source = m_jobs.fromString<Sources::Source>(name);
                if(source){
                    m_jobs.remove(name);
                }
            }
            break;
        }
        default:
            Debug() << __FILE__ << __func__ << "Unimplemented";
            return false;
        }
        return true;
    }while(false);
    return false;
}

void GStreamer::GStreamer::getStatus(json::value *status)const
{
    LOCK;
    using namespace tao;
    json::value array(json::empty_array);
    for(typename ThreadWorker::jobs_type::value_type const& pipeline : m_jobs){
        json::value obj(json::empty_object);
        GStreamerPipe *pipe(dynamic_cast<GStreamerPipe*>(pipeline.second));
        if(!pipe)
            continue;
        obj.get_object().insert({"state",(guint32)pipe->getState()});
        array.get_array().push_back(obj);
    }
    status->get_object().insert({"gstreamer",array});
}

Sources::GStreamerSampleSource *GStreamer::addPipeline(std::string pipeline,std::string name,guint32 maxSampleSize)
{
    LOCK;
    Sources::GStreamerSampleSource *source = new Sources::GStreamerSampleSource(this,name,maxSampleSize);
    Debug() << __func__ << name.c_str() << ":" << pipeline.c_str();
    if(!source->src.open(pipeline)) {
        delete source;
        return nullptr;
    }
    m_jobs.add(name,source);
    source->src.grabFrame();           //preroll / alloc
    source->src.start();
    source->setMemory(Allocator(CPS_call_void(Buffers::registerSource,dynamic_cast<Sources::Source*>(source))));
    connections.insert({&source->src,source});
    Debug() << source << "added.";
    return source;
}

Sources::GStreamerOffsetSource *GStreamer::addStream(Relevance::arguments_type const& args,string pipeline,string name)
{
    LOCK;
    string _source(args.at("source"));
    Sources::GStreamerSampleSource *source(
        m_jobs.fromString<Sources::GStreamerSampleSource>(_source));
    if(!source)
        return nullptr;
    Sources::GStreamerOffsetSource *offset = new Sources::GStreamerOffsetSource(this,name);
    offset->src.args(args);
    Debug() << __func__ << _source << ":" << pipeline.c_str();
    if(!offset->src.open(pipeline.c_str())){
        delete offset;
        return nullptr;
    }
    m_jobs.add(name,offset);
    offset->src.start();
    source->setMemory(Allocator(CPS_call_void(Buffers::registerSource,dynamic_cast<Sources::Source*>(source))));
    connections.insert({&source->src,offset});
    Debug() << offset << "added.";
    return offset;
}

} // namespace GStreamer

} // namespace Pleg
