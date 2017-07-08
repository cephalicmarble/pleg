#include <pleg.h>
using namespace Pleg;
#include "tao/json.hpp"
using namespace tao;
#include <sstream>
#include <algorithm>
#include <png.h>
using namespace std;
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/functional/hash.hpp>
using namespace boost;
#include "response.h"
#include "byte_array.h"
#include "exception.h"
#include "request.h"
#include "socket.h"
#include "event.h"
#include "transform.h"
#include "factory.h"
#include "writer.h"
#include "buffer.h"
//#include "bluetooth.h"
#include "source.h"
#include "gstreamer.h"
//#include "device.h"
#include "application.h"
#include "log.h"
#include "files.h"
#include "writer.h"
#include "server.h"
using namespace drumlin;

namespace Pleg {

Response::~Response()
{
    if(writer)
        delete writer;
}

/**
 * @brief Response::writeResponse : virtual dummy
 */
void Response::writeResponse()
{
    getRequest()->getSocketRef().write(string("ARGLE BARGLE"));
}

void Response::getStatus(json::value *status)const
{
    json::object_t &obj(status->get_object());
    obj.insert({"url",getRequest()->getUrl()});
    getRequest()->getSocketRef().getStatus(*status);
}

Options::Options(Request *req) : Response(req)
{
    statusCode = "200 OK";
    headers << "Date: "+posix_time::to_iso_string(posix_time::microsec_clock::universal_time())
            << "Allow: OPTIONS, GET, HEAD, POST, PATCH"
            << "Access-Control-Allow-Headers: Origin,Content-Type,Accept,Range,X-Method,X-Quiet"
            << "Access-Control-Allow-Methods: POST,GET,HEAD,OPTIONS,PATCH"
            << "Content-Length: 0"
    ;
}

void Options::service()
{
}

Head::Head(Request *req) : Response(req)
{
    statusCode = "200 OK";
}

void Head::service()
{
    headers << "Content-Type: dunno/whatever";
}

/**
 * @brief Get::Get : only constructor
 * @param req Request*
 */
Get::Get(Request *req) :
    Response(req),
    Continuer<Buffers::findRelevant_t>()
{
    Debug() << "new" << req->getVerb() << req->getUrl();
}

/**
 * @brief Get::service : services GET request
 * At present, just writes relevant buffers
 */
void Get::service()
{
    statusCode = "200 OK";
}

/**
 * @brief Get::accept : continuation for the CPS_call above
 * @param Buffers::findRelevant_t&
 * @param buffers Buffers::buffer_vec_type*
 */
void Get::accept(Buffers::findRelevant_t &,Buffers::buffer_vec_type buffers)
{
    copy(buffers.begin(),buffers.end(),back_inserter(data));
}

void Get::_get()
{
    const Relevance *rel(getRequest()->getRelevance());
    Debug() << "get::service - " << *rel;
    data.clear();
    auto buffers(Pleg::Cache(CPS_call(this,Buffers::findRelevant,rel)));
    string header(getRequest()->getHeader("Content-Type"));
    if(algorithm::find_tail(header,4)=="json"){
        headers << "Content-Type: text/json";
        getWriter()->writeJson(data);
    }else if(rel->hasSource() && rel->getSource()->type == Source_Gstreamer){
        headers << "Content-Type: image/png"
                << "Cache-Control: no-cache"
        ;
        Sources::GStreamerSampleSource *gs(dynamic_cast<Sources::GStreamerSampleSource*>(rel->getSource()));
        if(!gs || gs->getSrc().getWidth() == -1 || gs->getSrc().getHeight() == -1){
            return;
        }
        //I420 to RGB32_Premultiplied
        const unsigned char *_data = data.back()->data<unsigned char>();
        int width = gs->getSrc().getWidth(),height = gs->getSrc().getHeight();

        char filename[512];
        strcpy(filename,(filesystem::temp_directory_path().string()+filesystem::path::preferred_separator+"Server-png-XXXXXX").c_str());
        mkstemp(filename);

        FILE *fp = fopen(filename, "wb");
        if(!fp) return;
        png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png) return;
        png_infop info = png_create_info_struct(png);
        if (!info) return;
        if (setjmp(png_jmpbuf(png))) return;
        png_init_io(png,fp);
        // Output is 8bit depth, RGBA format.
        png_set_IHDR(
            png,
            info,
            width, height,
            8,
            PNG_COLOR_TYPE_RGB,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT,
            PNG_FILTER_TYPE_DEFAULT
        );
        png_write_info(png, info);

        png_color_struct *row = (png_color_struct *)calloc(width,sizeof(png_color_struct));

        //        QImage image(ize(width,height),QImage::Format_RGB888);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const unsigned char r = _data[ y * width * 3 + x * 3 + 0];
                const unsigned char g = _data[ y * width * 3 + x * 3 + 1];
                const unsigned char b = _data[ y * width * 3 + x * 3 + 2];
                row[ x ] = {r,g,b};
            }
            png_write_row(png, (png_const_bytep)row);
        }
        //        QBuffer buffer;
        //        image.save(&buffer, "PNG");
        //        getRequest()->getSocket()->write(buffer.data());

//        png_write_image(png, const_cast<unsigned char**>(&_data));

        png_write_end(png, NULL);

        free(row);
        fclose(fp);

        ifstream file(filename);
        stringstream ss;
        ss << file.rdbuf();
        getRequest()->getSocketRef().write(ss.str());
        file.close();
        filesystem::remove(filesystem::path(filename));

    }else{
        headers << "Content-Type: application/raw";
        for(const Buffers::buffer_vec_type::value_type &buffer : data){
            if(buffer){
                Debug() << "process buffer " << buffer << buffer->getRelevance();
                getRequest()->getSocketRef().write(byte_array(buffer->data<void>(),buffer->length())); //raw
            }
        }
    }
}

void Get::_dir()
{
    Config::JsonConfig config(Config::files_config_file);
    string root(config["/files_root"].get_string());
    Relevance rel(getRequest()->getRelevanceRef());
    string dirpath(root+(rel.params.end()!=rel.params.find("r")?string(""+filesystem::path::preferred_separator)+rel.params.at("r"):""));
    json::value tree(json::empty_object);
    json::object_t &tree_obj(tree.get_object());
    tree_obj.insert({"type","directory"});
    string_list path(string_list::fromString(dirpath,"/"));
    tree_obj.insert({"name",path.back()});
    path.pop_back();
    tree_obj.insert({"path",algorithm::join(path,"/")});
    json::value children(json::empty_array);
    string error(Files::treeAt(&children,dirpath));
    tree_obj.insert({"children",children});
    tree_obj.insert({"root",true});
    if(error.length())
        getRequest()->getSocketRef().write(error);
    else
        getRequest()->getSocketRef().write(json::to_string(tree));
}

gint64 Get::readRange(ifstream &device,string mime,gint64 completeLength,string range)
{
    byte_array bytes;
    boost::regex rx("\\d+");

    if(boost::regex_match(range,rx)){
        device.seekg(lexical_cast<int>(range));
        char c;
        device >> c;
        bytes = ""+c;
    }else if(algorithm::starts_with(range,"-")){
        device.seekg(lexical_cast<int>(range));
        device >> bytes;
    }else if(algorithm::starts_with(range,"-")){
        device.seekg(0);
        size_t sz(lexical_cast<int>(range.substr(1)));
        char *pc=(char*)malloc(sz);
        device.read(pc,sz);
        bytes.append(pc);
        free(pc);
    }else{
        string_list rparts(string_list::fromString(range,"-"));
        device.seekg(lexical_cast<int>(rparts[0]));
        size_t sz(1+lexical_cast<int>(rparts[1])-lexical_cast<int>(rparts[0]));
        char *pc=(char*)malloc(sz);
        device.read(pc,sz);
        bytes.append(pc,sz);
        free(pc);
    }

    if(2==boundary.length()){
        boost::hash<string> string_hash;
        boundary += string_hash(bytes.string());
    }
    string thisBoundary(boundary+"\r\nContent-Type: "+mime+"\r\nContent-Range: bytes "+range+"/"+lexical_cast<string>(completeLength)+"\r\n\r\n");
    getRequest()->getSocketRef().write(thisBoundary);
    getRequest()->getSocketRef().write(bytes);

    return bytes.length();
}

void Get::_file()
{
    Config::JsonConfig config(Config::files_config_file);
    string root(config["/files_root"].get_string());
    Relevance const& rel(getRequest()->getRelevanceRef());
    string filepath;
    if(rel.params.end()==rel.params.find("r")){
        filepath = root+filesystem::path::preferred_separator+rel.arguments.at("name");
    }else if(Files::virtualFilePath(rel.params.at("r")).length(),true){
        filepath = rel.params.at("r")+filesystem::path::preferred_separator+rel.arguments.at("name");
    }else if(Files::virtualFilePath(filesystem::path::preferred_separator+rel.params.at("r")).length(),true){
        filepath = root+filesystem::path::preferred_separator+rel.params.at("r")+filesystem::path::preferred_separator+rel.arguments.at("name");
    }
    filesystem::path path(Files::virtualFilePath(filepath));
    if(!path.string().length()) {
        statusCode = "403 Forbidden";
        Log() << "Attempted to access illegal path.";
        return;
    }
    gint64 completeLength(filesystem::file_size(path));
    string mime("application/stuff");
    if(algorithm::find_tail(path.string(),3)=="mp4"){
        mime = "video/mp4";
    }else{
        mime = "text/text";
    }
    headers << string("Date: ")+posix_time::to_iso_string(posix_time::microsec_clock::universal_time());
    if(!filesystem::exists(path)){
        statusCode = "404 File not found : " + path.string();
        return;
    }
    ifstream fstream(path.string());
    if(!fstream.is_open()){
        statusCode = "404 Unreadable : " + path.string();
        return;
    }
    gint64 contentLength = 0;
    if(getRequest()->headers.end() != getRequest()->headers.find("Range")){
        statusCode = "206 Partial Content";
        string::size_type sz(string::npos);
        string rangeHeader;
        if(string::npos != (sz = getRequest()->headers["Range"].find_first_of("bytes="))){
            rangeHeader = getRequest()->headers["Range"].replace(sz,6,"");
        }else{
            rangeHeader = getRequest()->headers["Range"];
        }
        algorithm::trim(rangeHeader);
        if(string::npos != rangeHeader.find_first_of(",")){
            string_list ranges(string_list::fromString(rangeHeader,","));
            for(string const& range : ranges){
                contentLength += readRange(fstream,mime,completeLength,range);
            }
        }else{
            contentLength += readRange(fstream,mime,completeLength,rangeHeader);
        }
        headers << string("Content-Type: multipart/byteranges; boundary=")+boundary.substr(2);
    }else{
        std::stringstream ss;
        ss << fstream.rdbuf();
        contentLength = getRequest()->getSocketRef().write(ss.str());
    }
    std::stringstream ss;
    ss << "Content-Length: " << contentLength;
    headers << ss.str();
    fstream.close();
}

void Get::_lsof()
{
    Config::JsonConfig config(Config::files_config_file);
    string root(config["/files_root"].get_string());
    Relevance const& rel(getRequest()->getRelevanceRef());
    string rpath(Files::virtualFilePath(root+filesystem::path::preferred_separator+(rel.params.end()!=rel.params.find("r")?rel.params.at("r"):string("")),true));
    if(!rpath.length()){
        statusCode = "403 Forbidden";
        Log() << "Attempted to access illegal path.";
        return;
    }
    json::value object(json::empty_object);
    json::object_t &obj(object.get_object());
    Files::Files::writers_vec_type writers(Files::files.list());
    for(Files::Files::writers_vec_type::value_type const& fileWriter : writers){
        string s(fileWriter->getFilePath());
        if(!algorithm::starts_with(s,rpath))
            continue;
        if(rel.arguments.end() != rel.arguments.find("name")
                &&
            filesystem::path(fileWriter->getFilePath()).filename() != rel.arguments.at("name")){
            continue;
        }
        json::value file(json::empty_object);
        fileWriter->getStatus(&file);
        obj.insert({fileWriter->getFilePath(),file});
    }
    string s(json::to_string(object));
    getRequest()->getSocketRef().write(byte_array(s.c_str(),s.length()));
}

/**
 * @brief Post::Post : only constructor
 * @param req Request*
 */
Post::Post(Request *req) : Response(req)
{
    Debug() << "new" << req->getVerb() << req->getUrl();
}

/**
 * @brief Post::service : services POST request
 */
void Post::service()
{
    statusCode = "200 OK";
    headers.push_back("Content-Type: text/json");
//    auto t(Factory::Transform::createPassthrough(this));
//    t->setWriter(getWriter());
//    getRequest()->getServer()->addThread(t->getThread(),true);
}

void Post::_touch()
{
    Config::JsonConfig config(Config::files_config_file);
    string root(config["/files_root"].get_string());
    Relevance const& rel(getRequest()->getRelevanceRef());
    string rpath(Files::virtualFilePath(rel.params.end()!=rel.params.find("r")&&rel.params.at("r").length()?rel.params.at("r")+filesystem::path::preferred_separator:string(""))+rel.arguments.at("file"));
    if(!rpath.length()){
        statusCode = "403 Forbidden";
        Log() << "Outside files_root!";
        return;
    }
    if(filesystem::exists(filesystem::path(rpath))){
        statusCode = "409 Conflict : File already exists";
        Log() << "File already exists.";
        return;
    }
    ofstream ofstrm;
    ofstrm.open(rpath,ios_base::out|ios_base::app);
    if(!ofstrm.is_open()){
        statusCode = "403 Forbidden";
        Log() << "EPERM";
        return;
    }
    ofstrm.close();
    Log() << "File" << rpath << "created";
    getRequest()->getSocketRef().write(string("File created"));
}

void Post::_mkdir()
{
    Config::JsonConfig config(Config::files_config_file);
    string root(config["/files_root"].get_string());
    Relevance const& rel(getRequest()->getRelevanceRef());
    string rpath(Files::virtualFilePath(filesystem::path::preferred_separator+(rel.params.end()!=rel.params.find("r")&&rel.params.at("r").length()?rel.params.at("r")+filesystem::path::preferred_separator:string("")),true));
    if(!rpath.length()){
        statusCode = "403 Forbidden";
        Log() << "Outside files_root!";
        return;
    }
    filesystem::path p(rpath);
    if(!filesystem::exists(p) || filesystem::status(p).type() ^ filesystem::directory_file)
    {
        statusCode = "404 Not found";
        Log() << "directory not found";
        return;
    }
    if(!filesystem::create_directory(rpath+filesystem::path::preferred_separator+rel.arguments.at("file"))){
        statusCode = "403 Forbidden";
        Log() << "mkdir failed";
        return;
    }
    Log() << "Directory" << rel.arguments.at("file") << "created in" << rpath;
    getRequest()->getSocketRef().write(string("Directory Created"));
}

void Post::makeWriterFile()
{
    getRequest()->delayed = true;
    Config::JsonConfig config(Config::files_config_file);
    string root(config["/files_root"].get_string());
    Relevance const& rel(getRequest()->getRelevanceRef());
    string rpath(Files::virtualFilePath(root+filesystem::path::preferred_separator+(rel.params.end()!=rel.params.find("r")?rel.params.at("r")+filesystem::path::preferred_separator:string(""))+rel.arguments.at("file")));
    if(!rpath.length()){
        statusCode = "403 Forbidden";
        Log() << "Outside files_root!";
        return;
    }
    Log() << rel.getSourceName() << "writing to" << rpath;
    rel.getSource()->writeToFile(getRequest(),rpath);
}

void Post::stopWriterFile()
{
    getRequest()->delayed = true;
    Config::JsonConfig config(Config::files_config_file);
    string root(config["/files_root"].get_string());
    Relevance const& rel(getRequest()->getRelevanceRef());
    string rpath(Files::virtualFilePath(root+filesystem::path::preferred_separator+(rel.params.end()!=rel.params.find("r")?rel.params.at("r")+filesystem::path::preferred_separator:string(""))+rel.arguments.at("file")));
    if(!rpath.length()){
        statusCode = "403 Forbidden";
        Log() << "Outside files_root!";
        return;
    }
    Log() << "removing file writer:" << rpath;
    Files::files.remove(rpath);
}

void Post::teeSourcePort()
{
    getRequest()->delayed = true;
    Relevance const& rel(getRequest()->getRelevanceRef());
    Sources::GStreamerSampleSource *source = dynamic_cast<Sources::GStreamerSampleSource*>(rel.getSource());
    threads_type threads(app->findThread("gstreamer",ThreadType_gstreamer));
    if(!source){
        getRequest()->getSocketRef().write(string("Not a sample source!"));
        Log() << rel.getSourceName() << "is not a sample source!";
        return;
    }
    if(0==std::distance(threads.begin(),threads.end())){
        getRequest()->getSocketRef().write(string("GStreamer not found!"));
        Log() << "GStreamer not found!";
        return;
    }
    for(threads_type::iterator::value_type const& thread : threads){
        GStreamer::GStreamer *gst(dynamic_cast<GStreamer::GStreamer*>(thread->getWorker()));
        if(!gst)
            return;
        if(gst->getJobs().end() != std::find_if(gst->getJobs().begin(),gst->getJobs().end(),[rel](GStreamer::GStreamer::jobs_type::value_type const& job){
            return job.first == (rel.arguments.at("source")+".tee");
        })) return;
        Log() << "opening udp tee at" << rel.arguments.at("ip") << rel.arguments.at("port");
        make_pod_event(Event::Type::GstStreamPort,"openSourcePort",getRequest())->send(thread);
        break;
    }
}

/**
 * @brief Patch::Patch : only constructor
 * @param req Request*
 */
Patch::Patch(Request *req) : Response(req)
{
    req->verb = PATCH;
    Debug() << "new" << req->getVerb() << req->getUrl();
}

/**
 * @brief Patch::service : services PATCH requests
 */
void Patch::service()
{
    statusCode = "200 OK";
    headers.push_back("Content-Type: text/json");
}

template <class Response>
struct list_routes
{
    typedef Response response_type;
    typedef std::vector<Route<Response>> routes_type;
    list_routes(Request *req,bool detail)
        :routes(req->getServer()->getRoutes<response_type>()),m_detail(detail)
    {}
    routes_type routes;
    void operator()(json::value &array)
    {
        for(auto & r : routes){
            array.get_array().push_back(r.toString(m_detail));
        }
    }
    bool m_detail;
};

void Patch::_routes()
{
    json::value array(json::empty_array);
    Relevance const& rel(getRequest()->getRelevanceRef());
    bool detail(rel.arguments.end()!=rel.arguments.find("detail"));
    list_routes<Get> r1(getRequest(),detail);r1(array);
    list_routes<Post> r2(getRequest(),detail);r2(array);
    list_routes<Patch> r3(getRequest(),detail);r3(array);
    list_routes<Catch> r4(getRequest(),detail);r4(array);
    list_routes<Head> r5(getRequest(),detail);r5(array);
    list_routes<Options> r6(getRequest(),detail);r6(array);
    getRequest()->getSocketRef().write(json::to_string(array));
}

void Patch::_devices()
{
    Config::JsonConfig config("devices.json");
    std::stringstream ss;
    config.save(ss);
    getRequest()->getSocketRef().write(ss.str());
}

/**
 * @brief Patch::_status : services PATCH /status, returning { threads:[name,...],sources:[{name:"",index:int,charting:false},...] }
 */
void Patch::_status()
{
    json::value status(json::empty_object);
    getRequest()->getServer()->getStatus(&status);
    Sources::getStatus(&status);
    Files::getStatus(&status);
    Pleg::Cache(CPS_call_void(Buffers::getCacheStatus,&status));
    Pleg::Allocator(CPS_call_void(Buffers::getAllocatorStatus,&status));
    string s(json::to_string(status));
    getRequest()->getSocketRef().write(s);
}

void Patch::_meta()
{
    string const& source(getRequest()->getRelevanceRef().arguments.at("source"));
    getWriter()->writeMetaJson(source);
}

/**
 * @brief Patch::_connect : services PATCH /connect/<mac> by starting bluetooth thread and returning _status()
 * @param mac tring const&
 */
//void Patch::_connect()
//{
//    string const& mac(getRequest()->getRelevanceRef().arguments.at("mac"));
//    Log() << "Connecting all services on device" << mac.toStdString();
//    make_pod_event(Event::Type::BluetoothStartThread,"startThread",mac)->punt();
//    this_thread::sleep_for(1000000000);
//}

//void Patch::_connectSource()
//{
//    Relevance::arguments_type const& args(getRequest()->getRelevanceRef().arguments);
//    threads_type threads(app->findThread("all",ThreadWorker::ThreadType::bluez));
//    do{
//        if(distance(threads.begin(),threads.end())<1){
//            make_pod_event(Event::Type::BluetoothStartThread,"startThread",string("bluez"))->punt();
//            Log() << "Starting bluetooth thread.";
//            break;
//        }
//        Thread *thread(threads.at(0));
//        Bluetooth *blue = dynamic_cast<Bluetooth*>(thread->getWorker());
//        if(!blue){
//            Log() << "No bluetooth thread started!";
//            Critical() << "No bluetooth thread started!";
//            break;
//        }
//        BluetoothLEDevice *le_device(blue->findDevice(args.at("mac")));
//        if(!le_device){
//            json::value *devices(blue->conf.getDevice(args.at("mac")));
//            for(auto device : devices->get_object()){
//                blue->connectDevice(new json::value(device.second));
//                qLog() << "Connecting device" << args.at("mac").toStdString();
//            }
//            break;
//        }
//        le_device->connectService(args.at("name"));
//        qLog() << "Connecting service" << args.at("name").toStdString();
//        this_thread::sleep_for(5000000000);
//    }while(false);
//}

///**
// * @brief Patch::_disconnect : PATCH /disconnect/<mac> destroys the worker(s)
// * @param mac tring const&
// */
//void Patch::_disconnect()
//{
//    tring const& mac(getRequest()->getRelevanceRef().arguments.at("mac"));
//    qDebug() << "disconnecting" << mac;
//    Bluetooth *bluet(nullptr);
//    BluetoothLEDevice *dev(nullptr);
//    threads_type threads(app->findThread(mac,ThreadWorker::ThreadType::bluez));
//    if(0==std::distance(threads.begin(),threads.end())){
//        getRequest()->getSocket()->write(mac+" not found\n");
//        return;
//    }
//    for(auto thread : threads){
//        bluet = dynamic_cast<Bluetooth*>(thread->getWorker());
//        dev = bluet->findDevice(mac);
//        if(nullptr==dev){
//            continue;
//        }
//        qLog() << "Disconnecting" << mac.toStdString();
//        bluet->disconnectDevice(dev);
//    }
//}

///**
// * @brief Patch::_scan : PATCH /scan
// */
//void Patch::_scan()
//{
//    Log() << "Starting bluetooth scan...";
//    Debug() << "Patch starting scan...";
//    Thread *thread(new Thread("scan"));
//    BluetoothScanner *scanner = new BluetoothScanner(thread,"devices.json");
//    connect(scanner,SIGNAL(timedOut()),this,SLOT(timedOut()));
//    connect(scanner,SIGNAL(writeToSocket()),this,SLOT(writeToSocket()));
//    app->addThread(thread,true);
//    worker = scanner;
//    thread->wait();
//}

void Patch::_openPipe()
{
    threads_type threads(app->findThread("gstreamer",ThreadType_gstreamer));
    if(0==distance(threads.begin(),threads.end())){
        threads.push_back(getRequest()->getServer()->startGStreamer("gstreamer")->getThread());
    }

    Config::JsonConfig config(Config::gstreamer_config_file);

    for(threads_type::iterator::value_type const& thread : threads){
        GStreamer::GStreamer *gst(dynamic_cast<GStreamer::GStreamer*>(thread->getWorker()));
        if(!gst)
            continue;
        make_event(Event::Type::GstConnectPipeline,__func__,getRequest())->send(gst->getThread());
    }
}

/**
 * @brief Patch::_config : PATCH /config returns client.json
 */
void Patch::_config()
{
    string file;
    if(!getRequest()->getRelevanceRef().arguments.count("file")){
        file = "devices.json";
    }else{
        file = getRequest()->getRelevanceRef().arguments.at("file");
    }
    Log() << "GET config from" << file;
    Config::JsonConfig config(string(".")+filesystem::path::preferred_separator+file);
    std::stringstream ss;
    config.save(ss);
    getRequest()->getSocketRef().write(ss.str());
}

/**
 * @brief Patch::_shutdown : PATCH /shutdown gracefully heads for the pub
 */
void Patch::_shutdown()
{
    Log() << "Shutting down...";
    getRequest()->getSocketRef().write(string("Shutting down..."));
    main_server->closed = true;
    make_event(Event::Type::ApplicationClose,"shutting down")->punt();
}

/**
 * @brief Patch::_restart : PATCH /restart allows restarting the server
 */
void Patch::_restart()
{
    Log() << "Restarting...";
    getRequest()->getSocketRef().write(string("Restarting..."));
    main_server->closed = true;
    make_event(Event::Type::ApplicationClose,"restarting",(Object*)1UL)->punt();
}

/**
 * @brief Patch::_removeSource : PATCH /remove/<source> may result in device disconnection
 * @param source tring const&
 */
void Patch::_removeSource()
{
    string const& source_name(getRequest()->getRelevanceRef().arguments.at("source"));
    Sources::Source *source(Sources::sources.fromString(source_name));
    if(!source){
        statusCode = "404 Source not found";
        Log() << statusCode;
        return;
    }
    Sources::GStreamerSourceBase *gstreamer_source(dynamic_cast<Sources::GStreamerSourceBase*>(source));
    if(!gstreamer_source){
        Sources::sources.remove((const char*)*source);
        Log() << "Removed source " << (const char*)*source;
    }else{
        threads_type threads(app->findThread("gstreamer",ThreadType_gstreamer));
        if(0==distance(threads.begin(),threads.end())){
            statusCode = "404 GStreamer not active";
            Log() << statusCode;
            return;
        }
        make_pod_event(Event::Type::GstRemoveJob,__func__,gstreamer_source->getName())->send(threads[0]);
    }
}

/**
 * @brief Patch::timedOut : PATCH /scan
 */
void Patch::timedOut()
{
    Log() << "Scan timed out.";
    getRequest()->getSocketRef().write(string("Scan timed out."));
}

///**
// * @brief Patch::writeToSocket : PATCH /scan
// */
//void Patch::writeToSocket()
//{
//    BluetoothScanner *scanner(dynamic_cast<BluetoothScanner*>(worker));
//    if(scanner){
//        scanner->conf.save(getRequest()->getSocket());
//    }
//    worker->signalTermination();
//}

void Catch::catchall()
{
    statusCode = "200 OK";
    Get get(getRequest());
    string_list parts(string_list::fromString(getRequest()->getUrl(),"/"));
    Relevance *rel(getRequest()->getRelevance());
    rel->arguments.clear();
    rel->arguments.insert({"name",parts.back()});
    if(algorithm::find_tail(parts.back(),4)==".mp4"){
        headers.push_back("Content-Type: video/mp4");
    }else{
        headers.push_back("Content-type: text/json");
    }
    parts.pop_back();
    rel->params.clear();
    rel->params.insert({"r",parts.join("/")});
    get._file();
}

void Catch::service()
{
}

} // namespace Pleg
