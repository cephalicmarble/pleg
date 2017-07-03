#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include "server.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
using namespace std;
#include <boost/thread/lock_guard.hpp>
#include "exception.h"
#include "request.h"
#include "response.h"
#include "source.h"
#include "event.h"
//#include "bluetooth.h"
#include "gstreamer.h"
#include "cursor.h"
#include "jsonconfig.h"

recursive_mutex Server::mutex;

namespace Pleg {

/**
 * @brief Server::Server : only constructor
 * @param parent Object*
 */
Server::Server(int argc,char **argv,int port) : ApplicationBase(argc,argv),ServerBase(port)
{
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
 * @brief Server::select_route : find a route
 * @param request
 * @return
 */
Server::route_return_type Server::select_route(Request *request)
{
    lock_guard l(&mutex);
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
        Debug() << "Irrelevant HTTP";
        return {false,std::prev(routes.end())};
    }
    Debug() << it->parse_func.pattern;
    return {relevance,it};
}

/**
 * @brief Server::start : start the server
 */
void Server::start()
{
    Log() << "Started Server";
    lock_guard l(&app->thread_critical_section);
    defineRoutes();
    ServerBase::start();
}

void Server::stop()
{
    Log() << "Stopped Server";
    ServerBase::stop();
    qDebug() << "Removing sources...";
    Sources::sources.removeAll();
    ApplicationBase::stop();
}

void Server::writeLog()const
{
    lock_guard l(&mutex);
    ofstream logfile("./Server.log");
    if(logfile.open(ios_base::out|ios_base::app)){
        for(string const& str : log){
            logfile << str << endl;
        }
        logfile.flush();
        logfile.close();
    }
    log.clear();
}

/**
 * @brief Server::getStatus : return a list.join("\n") of running threads
 * @return const char*
 */
void Server::getStatus(json::value *status)const
{
    Base::getStatus(status);
    lock_guard l(&thread_critical_section); //Server inherits Application<T>

    json::value requests(json::empty_array);
    json::array_t &threads(status->get_object().at("threads").get_array());

    for(json::array_t::iterator::value_type &thread : threads){
        if(std::string::npos != thread.get_object().at("task").get_string().find("Request")){
            requests.get_array().push_back(json::value(thread));
        }
    }

    if(log.count()){
        json::value _log(json::empty_array);
        json::array_t &logs(_log.get_array());
        for(string const& str : log){
            logs.push_back(str);
        }
        status->get_object().insert({"log",_log});
        writeLog();
    }

    status->get_object().insert({"requests",requests});
}
