#include <pleg.h>
using namespace Pleg;
#include "connection.h"
#include "exception.h"
#include "server.h"
#include "request.h"
#include "response.h"
#include "source.h"
#include "uri.h"
#include "socket.h"
#include "event.h"
//#include "bluetooth.h"
#include "gstreamer.h"
#include "application.h"

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
#include "log.h"

QMutex Server::mutex(QMutex::Recursive);

/**
 * @brief Server::Server : only constructor
 * @param parent QObject*
 */
Server::Server(int &argc,char *argv[]) : Base(argc,argv)
{
    server = new TcpServer();
    connect(server,SIGNAL(incomingConnection(qintptr)),this,SLOT(incomingConnection(qintptr)));
    installEventFilter(this);
}

Server::~Server()
{
    delete server;
}

/**
 * @brief Server::defineRoutes
 *
 * usual routing uri parts definition
 */
void Server::defineRoutes()
{
    addRoute(Uri::parser("dir?r")              ,"_dir",Route::GET);
    addRoute(Uri::parser("file/{name?}?r")     ,"_file",Route::GET);
    addRoute(Uri::parser("lsof/{name?}?r-")    ,"_lsof",Route::GET);

    addRoute(Uri::parser("touch/{file?}?r-")   ,"_touch",Route::POST);
    addRoute(Uri::parser("mkdir/{file?}?r-")   ,"_mkdir",Route::POST);
    addRoute(Uri::parser("write/{source?}/{file?}?r-"),"makeWriterFile",Route::POST);
    addRoute(Uri::parser("stop/{source?}/{file?}?r-"),"stopWriterFile",Route::POST);
    addRoute(Uri::parser("tee/{source?}/{ip?}/{port?}"),"teeSourcePort",Route::POST);

    addRoute(Uri::parser("routes")             ,"_routes",Route::CONTROL);
    addRoute(Uri::parser("routes/{detail?}")   ,"_routes",Route::CONTROL);

    addRoute(Uri::parser("devices")            ,"_devices",Route::CONTROL);
    addRoute(Uri::parser("status")             ,"_status",Route::CONTROL);
    addRoute(Uri::parser("meta/{source?}")     ,"_meta",Route::CONTROL);
//    addRoute(Uri::parser("scan/{file?}")       ,"_scanToFile",Route::CONTROL);
    addRoute(Uri::parser("scan")               ,"_scan",Route::CONTROL);

    addRoute(Uri::parser("connect/{mac?}/{name?}"),"_connectSource",Route::CONTROL);
    addRoute(Uri::parser("connect/{mac?}")     ,"_connect",Route::CONTROL);
    addRoute(Uri::parser("disconnect/{mac?}")  ,"_disconnect",Route::CONTROL);
    addRoute(Uri::parser("remove/{source?}")   ,"_removeSource",Route::CONTROL);

    addRoute(Uri::parser("pipe/{name?}")       ,"_openPipe",Route::CONTROL);

    addRoute(Uri::parser("config/{file?}")     ,"_config",Route::CONTROL);

    addRoute(Uri::parser("shutdown")           ,"_shutdown",Route::CONTROL);
    addRoute(Uri::parser("restart")            ,"_restart",Route::CONTROL);

    addRoute(Uri::parser("{source?}?c-")          ,"_get",Route::GET);

    addRoute(Uri::parser(""),"catchall",Route::CATCH);
}

/**
 * @brief Server::incomingConnection
 *
 * @param socketDescriptor qintptr
 */
void Server::incomingConnection(qintptr socketDescriptor)
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
 * @brief Server::startRequestThread : convenient function
 * @param socketDescriptor qintptr
 */
void Server::startRequestThread(qintptr socketDescriptor)
{
    Thread *thread(new Thread(tring("Request:")+QTime::currentTime().toString()));
    Request *req = new Request(this,thread);
    req->setSocket(0,socketDescriptor);
    qDebug() << "Starting thread " << req << thread;
    app->addThread(req->getThread(),true);
}

/**
 * @brief Server::select_route : find a route
 * @param request
 * @return
 */
Server::route_return_type Server::select_route(Request *request)
{
    QMutexLocker ml(&mutex);
    Relevance relevance;
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
 * @brief Server::eventFilter : deal with events
 * @param obj QObject*
 * @param event QEvent*
 * @return bool
 */
bool Server::eventFilter(QObject *obj, QEvent *event)
{
    try{
        if((quint32)event->type() > (quint32)Event::Type::first
        && (quint32)event->type() < (quint32)Event::Type::last){
            Event *pevent(dynamic_cast<Event *>(event));
            if(!pevent)
                return false;
            switch(pevent->type()){
            case Event::Type::BluetoothStartThread:
            {
                tring mac(pod_event_cast<tring>(pevent)->getVal());
                tring task = tring("source:")+mac;
                Bluetooth *worker = startBluetooth(task.toStdString().c_str());
                while(!worker->getThread()->isStarted())
                    Thread::yieldCurrentThread();
                QRegExp rx("([a-fA-F0-9]{2}:?){6}");
                if(rx.exactMatch(mac) || mac == "mock" || mac == "all")
                    make_pod_event(Event::Type::BluetoothConnectDevices,"connectDevices",mac)->send(worker->getThread());
                break;
            }
            case Event::Type::ApplicationRestart:
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
 * @brief Server::start : start the server
 */
void Server::start()
{
    qLog() << "Started Server";
    QMutexLocker ml(&app->thread_critical_section);
    defineRoutes();
    server->listen(QHostAddress::AnyIPv4,4999);
}

void Server::stop()
{
    qLog() << "Stopped Server";
    server->close();
    qDebug() << "Removing sources...";
    Sources::sources.removeAll();
    Base::stop();
}

void Server::writeLog()const
{
    QFile logfile("./Server.log");
    if(logfile.open(QFile::WriteOnly|QFile::Append)){
        for(tring const& str : log){
            logfile.write((str+"\n").toStdString().c_str());
        }
        logfile.flush();
        logfile.close();
    }
}

/**
 * @brief Server::getStatus : return a list.join("\n") of running threads
 * @return const char*
 */
void Server::getStatus(json::value *status)const
{
    Base::getStatus(status);
    QMutexLocker ml(&thread_critical_section); //Server inherits Application<T>

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
        for(tring const& str : log){
            logs.push_back(str.toStdString());
        }
        status->get_object().insert({"log",_log});
        const_cast<tringList*>(&log)->clear();
    }

    status->get_object().insert({"requests",requests});
}
