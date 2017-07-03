#include <qs.h>
using namespace QS;
#include "tao/json.hh"
using namespace tao;
#include "response.h"
#include <QDataStream>
#include <QByteArray>
#include <QSize>
#include <QDir>
#include <QCryptographicHash>
#include <QBuffer>
#include <sstream>
#include <algorithm>
#include <png.h>
#include "exception.h"
#include "request.h"
#include "socket.h"
#include "event.h"
#include "transform.h"
#include "factory.h"
#include "writer.h"
#include "buffer.h"
#include "bluetooth.h"
#include "source.h"
#include "gstreamer.h"
#include "device.h"
#include "application.h"
#include "qslog.h"

QSResponse::~QSResponse()
{
    if(writer)
        delete writer;
}

/**
 * @brief QSResponse::writeResponse : virtual dummy
 */
void QSResponse::writeResponse()
{
    getRequest()->getSocket()->write("ARGLE BARGLE");
}

void QSResponse::getStatus(json::value *status)const
{
    json::object_t &obj(status->get_object());
    obj.insert({"url",getRequest()->getUrl().toStdString()});
    getRequest()->getSocket()->getStatus(status);
}

QSOptions::QSOptions(QSRequest *req) : QSResponse(req)
{
    statusCode = "200 OK";
    headers << "Date: "+QDateTime::currentDateTimeUtc().toString();
    headers << "Allow: OPTIONS, GET, HEAD, POST, PATCH";
    headers << "Access-Control-Allow-Headers: Origin,Content-Type,Accept,Range,X-Method,X-Quiet";
    headers << "Access-Control-Allow-Methods: POST,GET,HEAD,OPTIONS,PATCH";
    headers << "Content-Length: 0";
}

void QSOptions::service()
{
}

QSHead::QSHead(QSRequest *req) : QSResponse(req)
{
    statusCode = "200 OK";
}

void QSHead::service()
{
    headers.push_back("Content-Type: dunno/whatever");
}

/**
 * @brief QSGet::QSGet : only constructor
 * @param req QSRequest*
 */
QSGet::QSGet(QSRequest *req) :
    QSResponse(req),
    Continuer<Buffers::findRelevant_t>()
{
    qDebug() << "new" << req->getVerb() << req->getUrl();
}

/**
 * @brief QSGet::service : services GET request
 * At present, just writes relevant buffers
 */
void QSGet::service()
{
    statusCode = "200 OK";
}

/**
 * @brief QSGet::accept : continuation for the CPS_call above
 * @param Buffers::findRelevant_t&
 * @param buffers Buffers::buffer_vec_type*
 */
void QSGet::accept(Buffers::findRelevant_t &,Buffers::buffer_vec_type buffers)
{
    std::copy(buffers.begin(),buffers.end(),std::back_inserter(data));
}

void QSGet::_get()
{
    const QSRelevance *rel(getRequest()->getRelevance());
    qDebug() << "get::service - " << *rel;
    data.clear();
    auto buffers(Buffers::Cache(CPS_call(this,Buffers::findRelevant,rel)));

    if(getRequest()->getHeader("Content-Type").endsWith("json")){
        headers.push_back("Content-Type: text/json");
        getWriter()->writeJson(data);
    }else if(rel->hasSource() && rel->getSource()->type == Sources::QSSource::Type::Video){
        headers.push_back("Content-Type: image/png");
        headers.push_back("Cache-Control: no-cache");
        Sources::QSGStreamerSource *gs(dynamic_cast<Sources::QSGStreamerSource*>(rel->getSource()));
        if(!gs){
            //FIXME error
            return;
        }
        //I420 to RGB32_Premultiplied
        const unsigned char *_data = data.back()->data<unsigned char>();
        int width = gs->getSrc()->getWidth(),height = gs->getSrc()->getHeight();

        char filename[512];
        strcpy(filename,(QDir::tempPath()+QDir::separator()+"QSServer-png-XXXXXX").toStdString().c_str());
        mktemp(filename);

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

        png_color_struct row[width];

        //        QImage image(QSize(width,height),QImage::Format_RGB888);
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

        fclose(fp);

        QFile file(filename);
        file.open(QFile::ReadOnly);
        getRequest()->getSocket()->write(file.readAll());
        file.close();
        QDir::temp().remove(filename);

    }else{
        headers.push_back("Content-Type: application/raw");
        for(const Buffers::buffer_vec_type::value_type &buffer : data){
            if(buffer){
                qDebug() << "process buffer " << buffer << buffer->getRelevance();
                getRequest()->getSocket()->write(buffer); //raw
            }
        }
    }
}

void QSGet::_dir()
{
    Config::QSJsonConfig config(Config::files_config_file);
    QString root(config["/files_root"].get_string().c_str());
    QSRelevance rel(getRequest()->getRelevanceRef());
    QString dirpath(root+(rel.params.end()!=rel.params.find("r")?QDir::separator()+rel.params.at("r"):""));
    json::value tree(json::empty_object);
    json::object_t &tree_obj(tree.get_object());
    tree_obj.insert({"type","directory"});
    QStringList path(dirpath.split("/",QString::SkipEmptyParts));
    tree_obj.insert({"name",path.back().toStdString()});
    path.pop_back();
    tree_obj.insert({"path",path.join("/").toStdString()});
    json::value children(json::empty_array);
    QString error(Files::treeAt(&children,dirpath));
    tree_obj.insert({"children",children});
    tree_obj.insert({"root",true});
    if(error.length())
        getRequest()->getSocket()->write(error);
    else
        getRequest()->getSocket()->write(json::to_string(tree).c_str());
}

qint64 QSGet::readRange(QIODevice *device,QString mime,qint64 completeLength,QString range)
{
    QByteArray bytes;

    if(QRegExp("\\d+").exactMatch(range)){
        device->seek(range.toInt());
        bytes = device->read(1);
    }else if(range.endsWith('-')){
        device->seek(range.toInt());
        bytes = device->readAll();
    }else if(range.startsWith('-')){
        device->seek(0);
        bytes = device->read(range.replace("-","").toInt());
    }else{
        QVector<QStringRef> rparts(range.splitRef("-",QString::SplitBehavior::SkipEmptyParts));
        device->seek(rparts[0].toInt());
        bytes = device->read((rparts[1].toInt()-rparts[0].toInt()) +1);
    }

    if(2==boundary.length()){
        boundary += QCryptographicHash::hash(bytes,QCryptographicHash::Sha1).toHex();
    }
    QString thisBoundary(boundary+"\r\nContent-Type: "+mime+"\r\nContent-Range: bytes "+range+"/"+completeLength+"\r\n\r\n");
    getRequest()->getSocket()->write(thisBoundary);
    getRequest()->getSocket()->write(bytes);

    return bytes.length();
}

void QSGet::_file()
{
    Config::QSJsonConfig config(Config::files_config_file);
    QString root(config["/files_root"].get_string().c_str());
    QSRelevance const& rel(getRequest()->getRelevanceRef());
    QString filepath;
    if(rel.params.end()==rel.params.find("r")){
        filepath = root+QDir::separator()+rel.arguments.at("name");
    }else if(Files::virtualFilePath(rel.params.at("r")).length(),true){
        filepath = rel.params.at("r")+QDir::separator()+rel.arguments.at("name");
    }else if(Files::virtualFilePath(QDir::separator()+rel.params.at("r")).length(),true){
        filepath = root+QDir::separator()+rel.params.at("r")+QDir::separator()+rel.arguments.at("name");
    }
    QString path(Files::virtualFilePath(filepath));
    if(!path.length()) {
        statusCode = "403 Forbidden";
        qLog() << "Attempted to access illegal path.";
        return;
    }
    QFile file(path);
    qint64 completeLength(file.size());
    QString mime("application/stuff");
    if(path.endsWith("mp4")){
        mime = "video/mp4";
    }else{
        mime = "text/text";
    }
    headers.append(QString("Date: ")+QDateTime::currentDateTime().toUTC().toString());
    if(!file.exists()){
        statusCode = "404 File not found : " + path;
        return;
    }
    if(!file.open(QFile::ReadOnly)){
        statusCode = "404 Unreadable : " + path;
        return;
    }
    qint64 contentLength = 0;
    if(getRequest()->headers.contains("Range")){
        statusCode = "206 Partial Content";
        QString rangeHeader(getRequest()->headers["Range"].replace("bytes=","").trimmed());
        if(~rangeHeader.indexOf(',')){
            QStringList ranges = rangeHeader.split(",",QString::SplitBehavior::SkipEmptyParts);
            for(QString const& range : ranges){
                contentLength += readRange(&file,mime,completeLength,range);
            }
        }else{
            contentLength += readRange(&file,mime,completeLength,rangeHeader);
        }
        headers.append(QString("Content-Type: multipart/byteranges; boundary=")+boundary.mid(2));
    }else{
        contentLength = getRequest()->getSocket()->write(file.readAll());
    }
    std::stringstream ss;
    ss << "Content-Length: " << contentLength;
    headers.append(QString(ss.str().c_str()));
    file.close();
}

void QSGet::_lsof()
{
    Config::QSJsonConfig config(Config::files_config_file);
    QString root(config["/files_root"].get_string().c_str());
    QSRelevance const& rel(getRequest()->getRelevanceRef());
    QString rpath(Files::virtualFilePath(root+QDir::separator()+(rel.params.end()!=rel.params.find("r")?rel.params.at("r"):QString("")),true));
    if(!rpath.length()){
        statusCode = "403 Forbidden";
        qLog() << "Attempted to access illegal path.";
        return;
    }
    json::value object(json::empty_object);
    json::object_t &obj(object.get_object());
    QSFiles::writers_vec_type writers(Files::files.list());
    for(QSFiles::writers_vec_type::value_type const& fileWriter : writers){
        if(!fileWriter->getFilePath().startsWith(rpath))
            continue;
        if(rel.arguments.end() != rel.arguments.find("name")
                &&
           fileWriter->getFile().fileName()!= rel.arguments.at("name")){
            continue;
        }
        json::value file(json::empty_object);
        fileWriter->getStatus(&file);
        obj.insert({fileWriter->getFilePath().toStdString(),file});
    }
    getRequest()->getSocket()->write(json::to_string(object).c_str());
}

/**
 * @brief QSPost::QSPost : only constructor
 * @param req QSRequest*
 */
QSPost::QSPost(QSRequest *req) : QSResponse(req)
{
    qDebug() << "new" << req->getVerb() << req->getUrl();
}

/**
 * @brief QSPost::service : services POST request
 */
void QSPost::service()
{
    statusCode = "200 OK";
    headers.push_back("Content-Type: text/json");
//    auto t(Factory::Transform::createQSPassthrough(this));
//    t->setWriter(getWriter());
//    getRequest()->getServer()->addThread(t->getQSThread(),true);
}

void QSPost::_touch()
{
    Config::QSJsonConfig config(Config::files_config_file);
    QString root(config["/files_root"].get_string().c_str());
    QSRelevance const& rel(getRequest()->getRelevanceRef());
    QString rpath(Files::virtualFilePath(rel.params.end()!=rel.params.find("r")&&rel.params.at("r").length()?rel.params.at("r")+QDir::separator():QString(""))+rel.arguments.at("file"));
    if(!rpath.length()){
        statusCode = "403 Forbidden";
        qLog() << "Outside files_root!";
        return;
    }
    QFile file(rpath);
    if(file.exists()){
        statusCode = "409 Conflict : File already exists";
        qLog() << "File already exists.";
        return;
    }
    if(!file.open(QFile::WriteOnly|QIODevice::Append)){
        statusCode = "403 Forbidden";
        qLog() << "EPERM";
        return;
    }
    file.close();
    qLog() << "File" << rpath.toStdString() << "created";
    getRequest()->getSocket()->write("File created");
}

void QSPost::_mkdir()
{
    Config::QSJsonConfig config(Config::files_config_file);
    QString root(config["/files_root"].get_string().c_str());
    QSRelevance const& rel(getRequest()->getRelevanceRef());
    QString rpath(Files::virtualFilePath(QDir::separator()+(rel.params.end()!=rel.params.find("r")&&rel.params.at("r").length()?rel.params.at("r")+QDir::separator():QString("")),true));
    if(!rpath.length()){
        statusCode = "403 Forbidden";
        qLog() << "Outside files_root!";
        return;
    }
    QDir dir(rpath);
    if(!dir.exists()){
        statusCode = "404 Not found";
        qLog() << "directory not found";
        return;
    }
    if(!dir.mkdir(rel.arguments.at("file"))){
        statusCode = "403 Forbidden";
        qLog() << "mkdir failed";
        return;
    }
    qLog() << "Directory" << rel.arguments.at("file").toStdString() << "created in" << rpath.toStdString();
    getRequest()->getSocket()->write("Directory Created");
}

void QSPost::makeWriterFile()
{
    getRequest()->delayed = true;
    Config::QSJsonConfig config(Config::files_config_file);
    QString root(config["/files_root"].get_string().c_str());
    QSRelevance const& rel(getRequest()->getRelevanceRef());
    QString rpath(Files::virtualFilePath(root+QDir::separator()+(rel.params.end()!=rel.params.find("r")?rel.params.at("r")+QDir::separator():QString(""))+rel.arguments.at("file")));
    if(!rpath.length()){
        statusCode = "403 Forbidden";
        qLog() << "Outside files_root!";
        return;
    }
    qLog() << rel.getSourceName() << "writing to" << rpath.toStdString();
    rel.getSource()->writeToFile(getRequest(),rpath);
}

void QSPost::stopWriterFile()
{
    getRequest()->delayed = true;
    Config::QSJsonConfig config(Config::files_config_file);
    QString root(config["/files_root"].get_string().c_str());
    QSRelevance const& rel(getRequest()->getRelevanceRef());
    QString rpath(Files::virtualFilePath(root+QDir::separator()+(rel.params.end()!=rel.params.find("r")?rel.params.at("r")+QDir::separator():QString(""))+rel.arguments.at("file")));
    if(!rpath.length()){
        statusCode = "403 Forbidden";
        qLog() << "Outside files_root!";
        return;
    }
    qLog() << "removing file writer:" << rpath.toStdString();
    Files::files.remove(rpath);
}

#if defined(GSTREAMER) || defined(QTGSTREAMER)
void QSPost::teeSourcePort()
{
    getRequest()->delayed = true;
    QSRelevance const& rel(getRequest()->getRelevanceRef());
    Sources::QSGStreamerSource *source = dynamic_cast<Sources::QSGStreamerSource*>(rel.getSource());
    threads_type threads(app->findThread("gstreamer",QSThreadWorker::QSThreadType::gstreamer));
    if(!source){
        getRequest()->getSocket()->write("Not a video source!");
        qLog() << rel.getSourceName() << "is not a video source!";
        return;
    }
    if(0==std::distance(threads.begin(),threads.end())){
        getRequest()->getSocket()->write("GStreamer not found!");
        qLog() << "GStreamer not found!";
        return;
    }
    for(threads_type::iterator::value_type const& thread : threads){
        GStreamer::QSGStreamer *gst(dynamic_cast<GStreamer::QSGStreamer*>(thread->getWorker()));
        if(!gst)
            return;
        if(gst->getJobs().end() != std::find_if(gst->getJobs().begin(),gst->getJobs().end(),[rel](GStreamer::QSGStreamer::jobs_type::value_type const& job){
            return job.first == (rel.arguments.at("source")+".tee").toStdString();
        })) return;
        qLog() << "opening udp tee at" << rel.arguments.at("ip").toStdString() << rel.arguments.at("port").toStdString();
        make_pod_event(QSEvent::Type::GstStreamPort,"openSourcePort",getRequest())->send(thread);
        break;
    }
}

#endif

/**
 * @brief QSPatch::QSPatch : only constructor
 * @param req QSRequest*
 */
QSPatch::QSPatch(QSRequest *req) : QSResponse(req)
{
    req->verb = QSRoute::PATCH;
    qDebug() << "new" << req->getVerb() << req->getUrl();
}

/**
 * @brief QSPatch::service : services PATCH requests
 */
void QSPatch::service()
{
    statusCode = "200 OK";
    headers.push_back("Content-Type: text/json");
}

void QSPatch::_routes()
{
    QSServer::routes_type const& routes(getRequest()->getServer()->getRoutes());
    json::value array(json::empty_array);
    QSRelevance const& rel(getRequest()->getRelevanceRef());
    bool detail(rel.arguments.end()!=rel.arguments.find("detail"));
    for(QSServer::routes_type::value_type const& r : routes){
        array.get_array().push_back(r.toStdString(detail));
    }
    getRequest()->getSocket()->write(json::to_string(array).c_str());
}

void QSPatch::_devices()
{
    Config::QSJsonConfig config("devices.json");
    config.save(getRequest()->getSocket());
}

/**
 * @brief QSPatch::_status : services PATCH /status, returning { threads:[name,...],sources:[{name:"",index:int,charting:false},...] }
 */
void QSPatch::_status()
{
    json::value status(json::empty_object);
    getRequest()->getServer()->getStatus(&status);
    Sources::getStatus(&status);
    Files::getStatus(&status);
    Buffers::Cache(CPS_call_void(Buffers::getCacheStatus,&status));
    Buffers::Allocator(CPS_call_void(Buffers::getAllocatorStatus,&status));
    getRequest()->getSocket()->write(json::to_string(status).c_str());
}

void QSPatch::_meta()
{
    QString const& source(getRequest()->getRelevanceRef().arguments.at("source"));
    getWriter()->writeMetaJson(source);
}

/**
 * @brief QSPatch::_connect : services PATCH /connect/<mac> by starting bluetooth thread and returning _status()
 * @param mac QString const&
 */
void QSPatch::_connect()
{
    QString const& mac(getRequest()->getRelevanceRef().arguments.at("mac"));
    qLog() << "Connecting all services on device" << mac.toStdString();
    make_pod_event(QSEvent::Type::BluetoothStartThread,"startThread",mac)->punt();
    QSThread::msleep(1000);
}

void QSPatch::_connectSource()
{
    QSRelevance::arguments_type const& args(getRequest()->getRelevanceRef().arguments);
    threads_type threads(app->findThread("all",QSThreadWorker::QSThreadType::bluez));
    do{
        if(std::distance(threads.begin(),threads.end())<1){
            make_pod_event(QSEvent::Type::BluetoothStartThread,"startThread",QString("bluez"))->punt();
            qLog() << "Starting bluetooth thread.";
            break;
        }
        QSThread *thread(threads.at(0));
        QSBluetooth *blue = dynamic_cast<QSBluetooth*>(thread->getWorker());
        if(!blue){
            qLog() << "No bluetooth thread started!";
            qCritical() << "No bluetooth thread started!";
            break;
        }
        QSBluetoothLEDevice *le_device(blue->findDevice(args.at("mac")));
        if(!le_device){
            json::value *devices(blue->conf.getDevice(args.at("mac")));
            for(auto device : devices->get_object()){
                blue->connectDevice(new json::value(device.second));
                qLog() << "Connecting device" << args.at("mac").toStdString();
            }
            break;
        }
        le_device->connectService(args.at("name"));
        qLog() << "Connecting service" << args.at("name").toStdString();
        QSThread::msleep(5000);
    }while(false);
}

/**
 * @brief QSPatch::_disconnect : PATCH /disconnect/<mac> destroys the worker(s)
 * @param mac QString const&
 */
void QSPatch::_disconnect()
{
    QString const& mac(getRequest()->getRelevanceRef().arguments.at("mac"));
    qDebug() << "disconnecting" << mac;
    QSBluetooth *bluet(nullptr);
    QSBluetoothLEDevice *dev(nullptr);
    threads_type threads(app->findThread(mac,QSThreadWorker::QSThreadType::bluez));
    if(0==std::distance(threads.begin(),threads.end())){
        getRequest()->getSocket()->write(mac+" not found\n");
        return;
    }
    for(auto thread : threads){
        bluet = dynamic_cast<QSBluetooth*>(thread->getWorker());
        dev = bluet->findDevice(mac);
        if(nullptr==dev){
            continue;
        }
        qLog() << "Disconnecting" << mac.toStdString();
        bluet->disconnectDevice(dev);
    }
}

/**
 * @brief QSPatch::_scan : PATCH /scan
 */
void QSPatch::_scan()
{
    qLog() << "Starting bluetooth scan...";
    qDebug() << "QSPatch starting scan...";
    QSThread *thread(new QSThread("scan"));
    QSBluetoothScanner *scanner = new QSBluetoothScanner(thread,"devices.json");
    connect(scanner,SIGNAL(timedOut()),this,SLOT(timedOut()));
    connect(scanner,SIGNAL(writeToSocket()),this,SLOT(writeToSocket()));
    app->addThread(thread,true);
    worker = scanner;
    thread->wait();
}

void QSPatch::_openPipe()
{
    QSRelevance const& rel(getRequest()->getRelevanceRef());
    threads_type threads(app->findThread("gstreamer",QSThreadWorker::QSThreadType::gstreamer));
    if(0==std::distance(threads.begin(),threads.end())){
        threads.push_back(getRequest()->getServer()->startGStreamer("gstreamer")->getQSThread());
    }

    Config::QSJsonConfig config(Config::gstreamer_config_file);

    for(threads_type::iterator::value_type const& thread : threads){
        GStreamer::QSGStreamer *gst(dynamic_cast<GStreamer::QSGStreamer*>(thread->getWorker()));
        if(!gst)
            continue;
        make_pod_event(QSEvent::Type::GstConnectPipeline,__func__,getRequest())->send(gst);
    }
}

/**
 * @brief QSPatch::_config : PATCH /config returns client.json
 */
void QSPatch::_config()
{
    std::string file;
    if(!getRequest()->getRelevanceRef().arguments.count("file")){
        file = "devices.json";
    }else{
        file = getRequest()->getRelevanceRef().arguments.at("file").toStdString();
    }
    qLog() << "GET config from" << file;
    Config::QSJsonConfig config(std::string("./")+file);
    config.save(getRequest()->getSocket());
}

/**
 * @brief QSPatch::_shutdown : PATCH /shutdown gracefully heads for the pub
 */
void QSPatch::_shutdown()
{
    qLog() << "Shutting down...";
    getRequest()->getSocket()->write("Shutting down...");
    main_server->closed = true;
    make_event(QSEvent::Type::ApplicationClose,"shutting down")->punt();
}

/**
 * @brief QSPatch::_restart : PATCH /restart allows restarting the server
 */
void QSPatch::_restart()
{
    qLog() << "Restarting...";
    getRequest()->getSocket()->write("Restarting...");
    main_server->closed = true;
    make_event(QSEvent::Type::ApplicationClose,"restarting",(QObject*)1UL)->punt();
}

/**
 * @brief QSPatch::_removeSource : PATCH /remove/<source> may result in device disconnection
 * @param source QString const&
 */
void QSPatch::_removeSource()
{
    QString const& source_name(getRequest()->getRelevanceRef().arguments.at("source"));
    Sources::QSSource *source(Sources::sources.fromString(source_name));
    if(!source){
        statusCode = "404 Source not found";
        qLog() << statusCode.toStdString();
        return;
    }
    Sources::QSGStreamerSourceBase *gstreamer_source(dynamic_cast<Sources::QSGStreamerSourceBase*>(source));
    if(!gstreamer_source){
        Sources::sources.remove((const char*)*source);
        qLog() << "Removed source " << (const char*)*source;
    }else{
        threads_type threads(app->findThread("gstreamer",QSThreadWorker::QSThreadType::gstreamer));
        if(0==std::distance(threads.begin(),threads.end())){
            statusCode = "404 GStreamer not active";
            qLog() << statusCode.toStdString();
            return;
        }
        make_pod_event(QSEvent::Type::GstRemoveJob,__func__,gstreamer_source->getName())->send(threads[0]);
    }
}

/**
 * @brief QSPatch::timedOut : PATCH /scan
 */
void QSPatch::timedOut()
{
    qLog() << "Scan timed out.";
    getRequest()->getSocket()->write("Scan timed out.");
}

/**
 * @brief QSPatch::writeToSocket : PATCH /scan
 */
void QSPatch::writeToSocket()
{
    QSBluetoothScanner *scanner(dynamic_cast<QSBluetoothScanner*>(worker));
    if(scanner){
        scanner->conf.save(getRequest()->getSocket());
    }
    worker->signalTermination();
}

void QSDummy::catchall()
{
    statusCode = "200 OK";
    QSGet get(getRequest());
    QStringList parts(getRequest()->getUrl().split("/"));
    QSRelevance *rel(getRequest()->getRelevance());
    rel->arguments.clear();
    rel->arguments.insert({"name",parts.back()});
    if(parts.back().endsWith("mp4")){
        headers.push_back("Content-Type: video/mp4");
    }else{
        headers.push_back("Content-type: text/json");
    }
    parts.pop_back();
    rel->params.clear();
    rel->params.insert({"r",parts.join("/")});
    get._file();
}

void QSDummy::service()
{
}
