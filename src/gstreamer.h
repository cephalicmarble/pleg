#ifndef GSTREAMER_H
#define GSTREAMER_H

#include "tao_forward.h"
using namespace tao;
#include <mutex>
using namespace std;
#include <math.h>
#include "thread.h"
#include "buffer.h"
#include "registry.h"
using namespace drumlin;

#define VERSION_NUM(major, minor, micro) (major * 1000000 + minor * 1000 + micro)
#define FULL_GST_VERSION VERSION_NUM(GST_VERSION_MAJOR, GST_VERSION_MINOR, GST_VERSION_MICRO)

#include <unistd.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/gstbuffer.h>
#include <gst/video/video.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/riff/riff-media.h>
#include <gst/pbutils/missing-plugins.h>

#if FULL_GST_VERSION >= VERSION_NUM(0,10,32)
#include <gst/pbutils/encoding-profile.h>
//#include <gst/base/gsttypefindhelper.h>
#endif

#if GST_VERSION_MAJOR == 0
    #define GstSample GstBuffer
#endif

#if defined(_WIN32) || defined(_WIN64)
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#include <sys/stat.h>
#endif

#define PipelineStates ( \
    StateNull,\
    StateReady,\
    StatePaused,\
    StatePlaying\
)
ENUM(PipelineState,PipelineStates)

namespace Pleg {

namespace Sources {
class GStreamerSampleSource;
class GStreamerOffsetSource;
class GStreamerSourceBase;
class SourceRegistry;
}

namespace GStreamer {

class GStreamerSrc;
class GStreamerSink;
class GStreamer;
class GStreamerStreamSource;

/**
 * @brief The gst_initializer class
 * Initializes gstreamer once in the whole process
 */
class gst_initializer
{
    static std::mutex gst_initializer_mutex;
public:
    gst_initializer(int *argc,char **argv[])
    {
        gst_initializer_mutex.lock();
        gst_init(argc,argv);
//        gst_debug_set_active(1);
//        gst_debug_set_colored(1);
//        gst_debug_set_default_threshold(GST_LEVEL_INFO);
        gst_initializer_mutex.unlock();
    }
    ~gst_initializer()
    {
        gst_task_cleanup_all();
    }
};

gboolean bus_func(GstBus * bus, GstMessage * message, gpointer user_data);

class GStreamerPipe :
    public WorkObject
{
public:
    GStreamerPipe(GStreamer *gst,string _name);
    virtual ~GStreamerPipe();

    typedef PipelineState State;

    GStreamer *getGStreamer()const{return gstreamer;}
    GstState getState();
    void handleMessages();
    gboolean handleMessage(GstBus *bus, GstMessage *msg);
    bool isPlaying();
    virtual void stop();
    virtual void start();
    virtual void restart();
    virtual void started(){}

    friend class GStreamerSrc;
    friend class GStreamer;
    friend class GStreamerSink;
    friend class GStreamerStreamSource;
    friend class Sources::GStreamerSampleSource;
    friend class Sources::GStreamerOffsetSource;

    virtual void report(json::value *obj,WorkObject::ReportType type)const;
    virtual bool open( std::string _pipeline);
    string const& getName()const{return name;}
protected:
    void stateChange(GstState state);
    virtual void close();

    GStreamer *gstreamer;
    GstElement *pipeline;
    GstElement *element;
    std::string name;
    std::string pipestr;
    guint64 frames = 0;
    GstCaps*      caps;
    GstBus*       bus;
    gint64        duration;
    double        fps;
};

void eos_callback(GstAppSink *,gpointer user_data);
GstFlowReturn new_preroll_callback(GstAppSink *,gpointer user_data);
GstFlowReturn new_sample_callback(GstAppSink *,gpointer user_data);
void      need_data_callback(GstAppSrc *src, guint length, gpointer user_data);
void      enough_data_callback(GstAppSrc *src, gpointer user_data);
gboolean  seek_data_callback(GstAppSrc *src, guint64 offset, gpointer user_data);

class GStreamerSrc :
        public Object,
        public GStreamerPipe
{
public:
    GStreamerSrc(GStreamer *gst,string name);
    bool grabFrame();
    virtual bool open(std::string _pipeline);
    gint getWidth()const{ return width; }
    gint getHeight()const{ return height; }

    friend class GStreamer;
    friend class GStreamerSink;
    friend class GStreamerStreamFiler;
    friend class Sources::GStreamerSampleSource;

    virtual void          eos          (){}
    virtual GstFlowReturn new_preroll  (){ return GST_FLOW_OK; }
    virtual GstFlowReturn new_sample   ();
protected:
    bool stopped = true;
    bool onlyOnce = false;
    gint          width;
    gint          height;
    int len;
    GstAppSinkCallbacks callbacks;
};

void toFraction(double decimal, double &numerator, double &denominator);
void handleMessage(GstElement * pipeline);

/**
 * @brief The GStreamerHttpStream class
 * Use Gstreamer to write video
 */
class GStreamerSink :
        public Object,
        public GStreamerPipe
{
public:
    GStreamerSink(GStreamer *gst,string name);

    virtual bool open( std::string filename );
    virtual void close();
    virtual void stop();
    friend class GStreamerStreamSource;

    virtual void need_data(guint32){}
    virtual void enough_data(){}
    virtual gboolean seek_data(guint64){ return false; }
    virtual void nextSample(GStreamerSrc *,GstSample *){}
protected:
    const char* filenameToMimetype(const char* filename);
    GstAppSrcCallbacks callbacks;
    GstElement *source = 0;
};

class GStreamerStreamSource : public GStreamerSink
{
public:
    GStreamerStreamSource(GStreamer *gst,string name);
    void start();
    void stop();
    void report(json::value *obj,WorkObject::ReportType type)const;
    void args(Relevance::arguments_type const& args);
    bool open(std::string filename);
    virtual void need_data(guint32 );
    virtual void enough_data();
    guint64 getTick()const{ return tick; }
    virtual void writeFrame(GstSample*);
    virtual void nextSample(GStreamerSrc *that,GstSample *sample);
public:
    bool going = false;
private:
    Relevance::arguments_type m_args;
    guint64 tick;
    guint64 frames;
};

/**
 * @brief The GStreamer class
 * Use GStreamer to capture video
 */
class GStreamer :
    public ThreadWorker
{
public:
    typedef map<GStreamerSrc*,Sources::GStreamerSourceBase*> connection_type;
    bool isDeleting()const{return deleting;}
    virtual ~GStreamer() {
        deleting = true;
        connections.clear();
        m_jobs.removeAll();
    }
    bool event(Event *event);
    virtual void shutdown();
    void getStatus(json::value *status)const;

    Sources::GStreamerSampleSource *addPipeline(string pipeline,string name,guint32 maxSampleSize);
    Sources::GStreamerOffsetSource *addStream(Relevance::arguments_type const& args,string pipeline,string name);
    void nextSample(GStreamerSrc *that,GstSample *sample);
    static GStreamer *getInstance(drumlin::Thread *_thread = nullptr){
        static GStreamer *_instance(nullptr);
        if(!_instance && _thread)
            _instance = new GStreamer(_thread);
        return _instance;
    }
private:
    GStreamer(drumlin::Thread *_thread):
        ThreadWorker(ThreadType_gstreamer,_thread)
    {
        m_type = ThreadType_gstreamer;
    }
    connection_type connections;
    bool deleting;
};

typedef std::pair<Sources::GStreamerSampleSource*,int> source_port_pair;

} // namespace GStreamer

} // namespace Pleg

#endif // GSTREAMER_H
