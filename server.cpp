#include <qs.h>
using namespace QS;
#include <QTcpServer>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QThreadPool>
#include <QException>
#include "exception.h"
#include "server.h"
#include "request.h"
#include "response.h"
#include "source.h"
#include "uri.h"
#include "socket.h"
#include "event.h"
#include "bluetooth.h"
#if defined(GSTREAMER) || defined(QTGSTREAMER)
#include "gstreamer.h"
#include "application.h"
#endif

#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include "cursor.h"
#include <tao/json.hh>
#include <tao/json/from_stream.hh>
using namespace std;
using namespace tao;
#include "jsonconfig.h"
#include "qslog.h"

QMutex QSServer::mutex(QMutex::Recursive);

/**
 * @brief QSServer::QSServer : only constructor
 * @param parent QObject*
 */
QSServer::QSServer(int &argc,char *argv[]) : Base(argc,argv)
{
    server = new QSTcpServer();
    connect(server,SIGNAL(incomingConnection(qintptr)),this,SLOT(incomingConnection(qintptr)));
    installEventFilter(this);
}

QSServer::~QSServer()
{
    delete server;
}

/**
 * @brief QSServer::defineRoutes
 *
 * usual routing uri parts definition
 */
void QSServer::defineRoutes()
{
    addRoute(Uri::parser("dir?r")              ,"_dir",QSRoute::GET);
    addRoute(Uri::parser("file/{name?}?r")     ,"_file",QSRoute::GET);
    addRoute(Uri::parser("lsof/{name?}?r-")    ,"_lsof",QSRoute::GET);

    addRoute(Uri::parser("touch/{file?}?r-")   ,"_touch",QSRoute::POST);
    addRoute(Uri::parser("mkdir/{file?}?r-")   ,"_mkdir",QSRoute::POST);
    addRoute(Uri::parser("write/{source?}/{file?}?r-"),"makeWriterFile",QSRoute::POST);
    addRoute(Uri::parser("stop/{source?}/{file?}?r-"),"stopWriterFile",QSRoute::POST);
    addRoute(Uri::parser("tee/{source?}/{ip?}/{port?}"),"teeSourcePort",QSRoute::POST);

    addRoute(Uri::parser("routes")             ,"_routes",QSRoute::CONTROL);
    addRoute(Uri::parser("routes/{detail?}")   ,"_routes",QSRoute::CONTROL);

    addRoute(Uri::parser("devices")            ,"_devices",QSRoute::CONTROL);
    addRoute(Uri::parser("status")             ,"_status",QSRoute::CONTROL);
    addRoute(Uri::parser("meta/{source?}")     ,"_meta",QSRoute::CONTROL);
//    addRoute(Uri::parser("scan/{file?}")       ,"_scanToFile",QSRoute::CONTROL);
    addRoute(Uri::parser("scan")               ,"_scan",QSRoute::CONTROL);

    addRoute(Uri::parser("connect/{mac?}/{name?}"),"_connectSource",QSRoute::CONTROL);
    addRoute(Uri::parser("connect/{mac?}")     ,"_connect",QSRoute::CONTROL);
    addRoute(Uri::parser("disconnect/{mac?}")  ,"_disconnect",QSRoute::CONTROL);
    addRoute(Uri::parser("remove/{source?}")   ,"_removeSource",QSRoute::CONTROL);

    addRoute(Uri::parser("pipe/{name?}")       ,"_openPipe",QSRoute::CONTROL);

    addRoute(Uri::parser("config/{file?}")     ,"_config",QSRoute::CONTROL);

    addRoute(Uri::parser("shutdown")           ,"_shutdown",QSRoute::CONTROL);
    addRoute(Uri::parser("restart")            ,"_restart",QSRoute::CONTROL);

    addRoute(Uri::parser("{source?}?c-")          ,"_get",QSRoute::GET);

    addRoute(Uri::parser(""),"catchall",QSRoute::CATCH);
}

/**
 * @brief QSServer::incomingConnection
 *
 * @param socketDescriptor qintptr
 */
void QSServer::incomingConnection(qintptr socketDescriptor)
{
    if(closed)
        return;
    try{
        startRequestThread(socketDescriptor);
    }catch(QException &e){
        qDebug() << e.what();
    }
}

/**
 * @brief QSServer::startBluetooth : bluetooth thread starter
 * @param task const char*
 * @return QSBluetooth*
 */
QSBluetooth *QSServer::startBluetooth(const char* task)
{
    QSThread *thread = new QSThread(task);
    QSBluetooth *bluet = new QSBluetooth(thread);
    qDebug() << "starting Bluetooth thread" << task << bluet << thread;
    app->addThread(thread,true);
    return bluet;
}

/**
 * @brief QSServer::startGstreamer : gstreamer thread starter
 * @param task const char*
 * @return QSGStreamer*
 */
#if defined(GSTREAMER) || defined(QTGSTREAMER)
GStreamer::QSGStreamer *QSServer::startGStreamer(const char* task)
{
    QSThread *thread = new QSThread(task);
    GStreamer::QSGStreamer *gst = new GStreamer::QSGStreamer(thread);
    qDebug() << "starting GStreamer thread" << task << gst << thread;
    app->addThread(thread,true);
    return gst;
}
#endif

/**
 * @brief QSServer::startRequestThread : convenient function
 * @param socketDescriptor qintptr
 */
void QSServer::startRequestThread(qintptr socketDescriptor)
{
    QSThread *thread(new QSThread(QString("Request:")+QTime::currentTime().toString()));
    QSRequest *req = new QSRequest(this,thread);
    req->setSocket(0,socketDescriptor);
    qDebug() << "Starting thread " << req << thread;
    app->addThread(req->getQSThread(),true);
}

/**
 * @brief QSServer::select_route : find a route
 * @param request
 * @return
 */
QSServer::route_return_type QSServer::select_route(QSRequest *request)
{
    QMutexLocker ml(&mutex);
    QSRelevance relevance;
    routes_type::iterator it(routes.end());
    for(routes_type::value_type &route : routes){
        if(route.method & request->getVerb()){
            relevance = route.parse_func(request->getUrl());
            if(relevance.toBool()){
                it = routes_type::iterator(&route);
                break;
            }
        }
    }
    if(it == routes.end()){
        qDebug() << "Irrelevant HTTP";
        return {false,std::prev(routes.end())};
    }
    qDebug() << it->parse_func.pattern;
    return {relevance,it};
}

/**
 * @brief QSServer::eventFilter : deal with events
 * @param obj QObject*
 * @param event QEvent*
 * @return bool
 */
bool QSServer::eventFilter(QObject *obj, QEvent *event)
{
    try{
        if((quint32)event->type() > (quint32)QSEvent::Type::first
        && (quint32)event->type() < (quint32)QSEvent::Type::last){
            QSEvent *pevent(dynamic_cast<QSEvent *>(event));
            if(!pevent)
                return false;
            switch(pevent->type()){
            case QSEvent::Type::BluetoothStartThread:
            {
                QString mac(pod_event_cast<QString>(pevent)->getVal());
                QString task = QString("source:")+mac;
                QSBluetooth *worker = startBluetooth(task.toStdString().c_str());
                while(!worker->getQSThread()->isStarted())
                    QSThread::yieldCurrentThread();
                QRegExp rx("([a-fA-F0-9]{2}:?){6}");
                if(rx.exactMatch(mac) || mac == "mock" || mac == "all")
                    make_pod_event(QSEvent::Type::BluetoothConnectDevices,"connectDevices",mac)->send(worker->getQSThread());
                break;
            }
            case QSEvent::Type::ApplicationRestart:
                start();
                break;
            default:
                return false;
            }
            return true;
        }
        return false;
    }catch(QException &e){
        qDebug() << e.what();
    }
    return false;
}

/**
 * @brief QSServer::start : start the server
 */
void QSServer::start()
{
    qLog() << "Started QSServer";
    QMutexLocker ml(&app->thread_critical_section);
    defineRoutes();
    server->listen(QHostAddress::AnyIPv4,4999);
}

void QSServer::stop()
{
    qLog() << "Stopped QSServer";
    server->close();
    qDebug() << "Removing sources...";
    Sources::sources.removeAll();
    Base::stop();
}

void QSServer::writeLog()const
{
    QFile logfile("./QSServer.log");
    if(logfile.open(QFile::WriteOnly|QFile::Append)){
        for(QString const& str : log){
            logfile.write((str+"\n").toStdString().c_str());
        }
        logfile.flush();
        logfile.close();
    }
}

/**
 * @brief QSServer::getStatus : return a list.join("\n") of running threads
 * @return const char*
 */
void QSServer::getStatus(json::value *status)const
{
    Base::getStatus(status);
    QMutexLocker ml(&thread_critical_section); //QSServer inherits QSApplication<T>

    json::value requests(json::empty_array);
    json::array_t &threads(status->get_object().at("threads").get_array());

    for(json::array_t::iterator::value_type &thread : threads){
        if(std::string::npos != thread.get_object().at("task").get_string().find("Request")){
            requests.get_array().push_back(json::value(thread));
        }
    }

    if(log.count()){
        writeLog();
        json::value _log(json::empty_array);
        json::array_t &logs(_log.get_array());
        for(QString const& str : log){
            logs.push_back(str.toStdString());
        }
        status->get_object().insert({"log",_log});
        const_cast<QStringList*>(&log)->clear();
    }

    status->get_object().insert({"requests",requests});
}
