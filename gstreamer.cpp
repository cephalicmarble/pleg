#include "pleg.h"
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include <functional>
using namespace std;
#include <boost/thread/lock_guard.hpp>
#include <boost/algorithm/string.hpp>
using namespace boost;
#include "gstreamer.h"
#include "jsonconfig.h"
#include "buffer.h"
#include "event.h"
#include "cursor.h"
#include "source.h"
#include "request.h"
using namespace drumlin;

/* Many thanks for the much-edited cap_gstreamer.cpp example, opencv (cephalicmarble@hotmail.com) */

namespace GStreamer {

void toFraction(double decimal, double &numerator, double &denominator);

boost::mutex gst_initializer::gst_initializer_mutex;

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

boost::mutex bus_mutex;
gboolean bus_func(GstBus * bus, GstMessage * msg, gpointer user_data)
{
    lock_guard l(&bus_mutex);
    GStreamerPipe *gp(dynamic_cast<GStreamerPipe*>(static_cast<Object*>(user_data)));
    if(!gp)
        return false;
    return gp->handleMessage(bus,msg);
}

GStreamerPipe::GStreamerPipe(GStreamer *gst):WorkObject(),gstreamer(gst)
{
    pipeline = 0;
    element = 0;
    caps = 0;
}

recursive_mutex mutex;
#define LOCK lock_guard l(&GStreamer::mutex);

GStreamerPipe::~GStreamerPipe()
{
    LOCK
    Debug() << this << __func__;
    close();
    if(!getGStreamer()->isDeleting())
        make_pod_event(Event::Type::GstRemoveJob,__func__,getName())->send(getGStreamer()->getThread());
}

void GStreamerPipe::handleMessages()
{
    LOCK
    lock_guard l2(&bus_mutex);
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg;
    while(gst_bus_have_pending(bus)) {
        msg = gst_bus_pop(bus);
        //printf("Got %s message\n", GST_MESSAGE_TYPE_NAME(msg));
        handleMessage(bus,msg);
    }
}

gboolean GStreamerPipe::handleMessage(GstBus *bus, GstMessage *msg)
{
    LOCK
    GError *err = NULL;
    gchar *debug = NULL;
    GstStreamStatusType tp;
    GstElement * elem = NULL;

    if(gst_is_missing_plugin_message(msg))
    {
        qDebug() << "GStreamer: your gstreamer installation is missing a required plugin";
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
            Debug() << "stream status: elem" << GST_ELEMENT_NAME(elem) << tp;
            break;
        case GST_MESSAGE_STREAM_START:
            gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
            break;
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
    LOCK
    if(pipeline) {
        gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
        gst_object_unref(GST_OBJECT(pipeline));
        pipeline = NULL;
    }
}

GstState GStreamerPipe::getState()
{
    GstState current,pending;
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
    LOCK
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
    LOCK
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
    LOCK
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
    LOCK
    Debug() << this << __func__;
    stateChange();
}

/**
 * @brief GStreamerSource::restartPipeline
 * Restart the pipeline
 */
void GStreamerPipe::restart()
{
    LOCK
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
bool GStreamerPipe::open( std::string const& filename )
{
    LOCK
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
    gchar* name = NULL;
    GValue value = G_VALUE_INIT;

    while (!done)
    {
        switch (gst_iterator_next (it, &value))
        {
        case GST_ITERATOR_OK:
            _element = GST_ELEMENT (g_value_get_object (&value));
            name = gst_element_get_name(_element);
            if (name)
            {
                if((done = (strstr(name, "sink0") != NULL)))
                {
                    element = GST_ELEMENT ( gst_object_ref (_element) );
                }
               else if((done = (strstr(name, "src0") != NULL)))
                {
                    element = GST_ELEMENT ( gst_object_ref (_element) );
                }
                g_free(name);
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
    if(!gst_element_query_duration(element, format, &duration))
    {
        Debug() << "GStreamer: unable to query duration of stream";
        duration = -1;
    }

    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_func, (gpointer)this);
    gst_object_unref (bus);

    return true;
}

void GStreamerPipe::report(json::value *_obj,ReportType type)const
{
    LOCK
    auto &obj(_obj->get_object());
    obj.insert({"pipeline",pipestr});
    if(nullptr == pipeline)return;
    GstState status;
    gst_element_get_state(pipeline, &status, NULL, GST_REASONABLE_WAIT);
    obj.insert({"state",QMetaEnum::fromType<GStreamerPipe::State>().valueToKey(status)});
    obj.insert({"frames",frames});
}

GStreamerSrc::GStreamerSrc(GStreamer *gst):Object(0),GStreamerPipe(gst)
{
    LOCK
    duration = -1;
    width = -1;
    height = -1;
    fps = -1;
    callbacks.eos = &eos_callback;
    callbacks.new_preroll = &new_preroll_callback;
    callbacks.new_sample = &new_sample_callback;
}

#define CALLBACK_LOCK static mutex s_mutex;lock_guard l(&mutex);
void eos_callback(GstAppSink *,gpointer user_data)
{
    CALLBACK_LOCK
    static_cast<GStreamerSrc*>(user_data)->eos();
}
GstFlowReturn new_preroll_callback(GstAppSink *,gpointer user_data)
{
    CALLBACK_LOCK
    return static_cast<GStreamerSrc*>(user_data)->new_preroll();
}
GstFlowReturn new_sample_callback(GstAppSink *,gpointer user_data)
{
    CALLBACK_LOCK
    return static_cast<GStreamerSrc*>(user_data)->new_sample();
}

void need_data_callback(GstAppSrc *src, guint length, gpointer user_data)
{
    CALLBACK_LOCK
    static_cast<GStreamerSink*>(user_data)->need_data(length);
}
void enough_data_callback(GstAppSrc *src, gpointer user_data)
{
    CALLBACK_LOCK
    static_cast<GStreamerSink*>(user_data)->enough_data();
}
gboolean seek_data_callback(GstAppSrc *src, guint64 offset, gpointer user_data)
{
    CALLBACK_LOCK
    return static_cast<GStreamerSink*>(user_data)->seek_data(offset);
}

GstFlowReturn GStreamerSrc::new_sample()
{
    frames++;
    grabFrame();
}

bool GStreamerSrc::open(std::string const& _pipeline)
{
    LOCK
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
    LOCK
    if(!pipeline){
        Critical() << "unexpectedly null pipeline";
        return false;
    }

    GstState state;
    GstState pending;
    GstSample *sample = 0;
    gst_element_get_state(pipeline, &state, &pending, GST_REASONABLE_WAIT);
    do{
        if(state != (GstState)GStreamerPipe::StatePlaying){
            if(!sample)
               sample = gst_app_sink_pull_preroll(GST_APP_SINK(element));
            stateChange(GST_STATE_PLAYING);
        }else{
            if(!sample)
                sample = gst_app_sink_pull_sample(GST_APP_SINK(element));
        }
        gst_element_get_state(pipeline, &state, &pending, 0.4 * 1000000000.);
    }while(!sample || state != (GstState)GStreamerPipe::StatePlaying);

    if(!sample){
        Critical() << "unexpectedly null sample.";
        return false;
    }

    GstBuffer *buffer = 0;

    if(!onlyOnce){
        GstPad* pad = gst_element_get_static_pad(element, "sink");
        GstCaps* buffer_caps = gst_pad_get_current_caps(pad);
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

        buffer = gst_sample_get_buffer(sample);
        if(!buffer){
            gst_sample_unref(sample);
            return false;
        }
        len = gst_buffer_get_size(buffer);

        gst_buffer_unref(buffer);

        onlyOnce = true;
    }

    getGStreamer()->nextSample(this,sample);

    gst_sample_unref(sample);

    return true;
}

GStreamerSink::GStreamerSink(GStreamer *gst)
    :Object(0),GStreamerPipe(gst)
{
    LOCK
    callbacks.need_data = &need_data_callback;
    callbacks.enough_data = &enough_data_callback;
    callbacks.seek_data = &seek_data_callback;
}

bool GStreamerSink::open( std::string const& _pipeline )
{
    LOCK
    if(!GStreamerPipe::open(_pipeline)){
        return false;
    }

    gst_app_src_set_emit_signals (GST_APP_SRC(element), 0);
    gst_app_src_set_callbacks(GST_APP_SRC(element),&callbacks,this,NULL);

    qDebug() << __func__ << _pipeline.c_str();

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
    LOCK
    while(pipeline)
    {
        if (gst_app_src_end_of_stream(GST_APP_SRC(element)) != GST_FLOW_OK)
        {
            qDebug() << "Cannot send EOS to GStreamer pipeline";
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
                qDebug() << "Error during VideoWriter finalization";
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
    LOCK
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
    LOCK
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
void GStreamerStreamSource::writeFrame( GObject *sample )
{
    LOCK
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

    //gst_app_src_push_buffer takes ownership of the buffer, so we need to supply it a copy
    GstBuffer *buffer = gst_sample_get_buffer(GST_SAMPLE(sample));
    if(!buffer){
        Debug() << "no buffer in sample";
        return;
    }
    GST_BUFFER_DURATION(buffer) = duration;
    GST_BUFFER_PTS(buffer) = timestamp;
    GST_BUFFER_DTS(buffer) = timestamp;
    //set the current number in the frame
    GST_BUFFER_OFFSET(buffer) =  tick++;
    GstBuffer *second = gst_buffer_copy_deep(buffer);
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

GStreamerStreamSource::GStreamerStreamSource(GStreamer *gst):GStreamerSink(gst),tick(0)
{
    LOCK
}

void GStreamerStreamSource::nextSample(GStreamerSrc *that,GstSample *sample)
{
    writeFrame(sample);
}

void GStreamerStreamSource::args(Relevance::arguments_type const& args)
{
    LOCK
    copy(args.begin(),args.end(),inserter(m_args,m_args.begin()));
}

bool GStreamerStreamSource::open(std::string const& filename)
{
    LOCK
    string pipeline = filename;
    for(auto & arg : m_args){
        string needle(arg.first);
        auto index(pipeline.find_last_of(needle));
        pipeline = pipeline.replace(index,needle.length(),arg.second);
    }
    return GStreamerSink::open(pipeline.c_str());
}

void GStreamerStreamSource::report(json::value *_obj,ReportType type)const
{
    LOCK
    GStreamerSink::report(_obj,type);
    json::value::object_t &obj(_obj->get_object());
    if(m_args.end()!=m_args.find("ip")){
        obj.insert({"udp",(m_args.at("ip")+":"+m_args.at("port")).toStdString()});
    }else{
        obj.insert({"filepath",filepath.toStdString()});
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
    LOCK
    GstPad *source_pad = gst_element_get_static_pad(GST_ELEMENT(src->element),"sink");
    if(!source_pad){
        handleMessages();
        return;
    }
    GstCaps *caps = gst_pad_get_current_caps(source_pad);
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

/**
 * @brief GStreamer::run : runnable function
 */
void GStreamer::run(QObject *obj,Event *event)
{
}

void GStreamer::nextSample(GStreamerSrc *that,GstSample *sample)
{
    LOCK
    for(auto & connection : connections){
        if(connection.first == that){
            connection.second->nextSample(that,sample);
        }
    }
}

/**
 * @brief GStreamer::shutdown : stop things
 */
void GStreamer::shutdown()
{
    LOCK
    qDebug() << this << __func__;
    connections.clear();
    for(jobs_type::value_type const& obj : jobs){
        obj.second->stop();
    }
    signalTermination();
}

/**
 * @brief GStreamer::gstreamerEventFilter
 * @param obj QObject*
 * @param event Event*
 */
bool GStreamer::event(QEvent *event)
{
    Event *pevent = dynamic_cast<Event*>(event);
    if(!pevent)
        return false;
    quietDebug() << this << __func__ << pevent->type();
    if((quint32)pevent->type() > (quint32)Event::Type::first
            && (quint32)pevent->type() < (quint32)Event::Type::last){
        switch(pevent->type()){
        case Event::Type::GstConnectDevices:
        {
            Config::JsonConfig gst_config("./gstreamer.json");
            json::value &pipes(gst_config["/pipes"]);
            for(auto it : pipes.get_object()){
                addPipeline(it.second[string("pipeline")].get_string(),it.first,it.second[string("maxSampleSize")].get_unsigned());
            }
            break;
        }
        case Event::Type::GstConnectPipeline:
        {
            Config::JsonConfig gst_config("./gstreamer.json");
            Request *request;
            pod_event_cast(pevent,&request);
            Relevance &rel(request->getRelevanceRef());
            json::value &pipe(config.at((QString("/pipes/")+rel.arguments.at("name"))));
            addPipeline(pipe[string("pipeline")].get_string(),rel.arguments.at("name"),pipe[string("maxSampleSize")].get_unsigned());
            make_event(pevent->type(),"open")->send(request->getThread());
            break;
        }
        case Event::Type::GstStreamPort:
        case Event::Type::GstStreamFile:
        {
            Config::JsonConfig gst_config("./gstreamer.json");
            Request *request;
            pod_event_cast(pevent,&request);
            Relevance &rel(request->getRelevanceRef());
            string suffix = pevent->type()==GstStreamPort?"tee":"file";
            string pipepath = string("/pipes/")+rel.arguments.at("name")+"/"+suffix;
            std::string pipeline = gst_config[pipepath].get_string();
            if(pipeline[0]=='@'){
                pipeline = gst_config[pipeline.substr(1)].get_string();
            }
            addStream(request->getRelevanceRef().arguments,pipeline,rel.arguments.at("name")+"."+suffix);
            make_event(pevent->type(),"open")->send(request->getThread());
            break;
        }
        case Event::Type::GstStreamEnd:
        {
            delete event_cast<QObject>(pevent)->getPointer();
            break;
        }
        case Event::Type::GstRemoveJob:
        {
            if(!isDeleting()){
                LOCK
                lock_guard l2(&getThread()->critical_section);
                jobs.remove(pod_event_cast<std::string>(pevent)->getVal().c_str());
            }
            break;
        }
        case Event::Type::ControlStartStream:
        {
            tring pipeline = pod_event_cast<tring>(pevent)->getVal();
            GStreamer::GStreamerSrc *stream(addVideoStream(pipeline.toStdString().c_str()));
            if(!stream){
                quietDebug() << "FAIL!";
            }else{
                make_event(Event::Type::ControlStreamStarted,"videostream",stream)->punt();
            }
            break;
        }
        default:
            qDebug() << __func__ << "Unimplemented";
            return false;
        }
        return true;
    }while(false);
    return false;
}

void GStreamer::GStreamer::getStatus(json::value *status)const
{
    using namespace tao;
    json::value array(json::empty_array);
    for(typename ThreadWorker::jobs_type::value_type const& pipeline : jobs){
        json::value obj(json::empty_object);
        GStreamer::GStreamerPipe *pipe(dynamic_cast<GStreamer::GStreamerPipe*>(pipeline.second));
        obj.get_object().insert({"state",(guint32)pipe->getState()});
    }
}

GStreamer::GStreamerSrc *GStreamer::addPipeline(std::string const& pipeline,std::string const& name,guint32 maxSampleSize)
{
    LOCK
    Sources::GStreamerSampleSource *source = new Sources::GStreamerSampleSource(this,name,maxSampleSize);
    Debug() << __func__ << name.c_str() << ":" << pipeline.c_str();
    if(!source->open(pipeline)) {
        delete source;
        return nullptr;
    }
    jobs.add(name,source->src);
    source->grabFrame();           //preroll / alloc
    Sources::sources.add(name,source);  //register
    source->start();               //signals and slots
    Debug() << source << "added.";
    return source;
}

GStreamer::GStreamerSink *GStreamer::addStream(Relevance::arguments_type const& args,string const& pipeline,string const& name)
{
    LOCK
    string _source(args.at("source"));
    Sources::GStreamerSampleSource *source(
        Sources::sources.fromString<Sources::GStreamerSampleSource>(_source));
    if(!source)
        return nullptr;
    GStreamer::GStreamerStreamSource *stream = new GStreamer::GStreamerStreamSource();
    stream->args(args);
    Debug() << __func__ << _source << ":" << pipeline.c_str();
    if(!stream->open(pipeline.c_str())){
        delete stream;
        return nullptr;
    }
    jobs.add(name,stream);
    stream->start();
    connections.insert({source,stream});
    Debug() << stream << "added.";
    return stream;
}

}

#endif // GSTREAMER
