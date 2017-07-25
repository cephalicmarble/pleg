#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include "server.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <mutex>
using namespace std;
#include "exception.h"
#include "request.h"
#include "response.h"
#include "source.h"
#include "event.h"
//#include "bluetooth.h"
#include "gstreamer.h"
#include "cursor.h"
#include "jsonconfig.h"
using namespace drumlin;

namespace Pleg {

/**
 * @brief Server::Server : only constructor
 * @param parent Object*
 */
Server::Server(int argc,char **argv,string address,int port) : ApplicationBase(argc,argv),ServerBase(address,port)
{
}

Server::~Server()
{
}

/**
 * @brief Server::defineRoutes
 *
 * usual routing uri parts definition
 */
void Server::defineRoutes()
{
    get(Uri::parser("dir?r-")                         ,&Get::_dir);
    get(Uri::parser("file/{name?}?r")                ,&Get::_file);
    get(Uri::parser("lsof/{name?}?r-")               ,&Get::_lsof);

    post(Uri::parser("touch/{file?}?r-")             ,&Post::_touch);
    post(Uri::parser("mkdir/{file?}?r-")             ,&Post::_mkdir);
    post(Uri::parser("write/{source?}/{file?}?r-")   ,&Post::makeWriterFile);
    post(Uri::parser("stop/{source?}/{file?}?r-")    ,&Post::stopWriterFile);
    post(Uri::parser("tee/{source?}/{@ip?}/{@port?}")  ,&Post::teeSourcePort);

    patch(Uri::parser("routes")                      ,&Patch::_routes);
    patch(Uri::parser("routes/{detail?}")            ,&Patch::_routes);

    patch(Uri::parser("devices")                     ,&Patch::_devices);
    patch(Uri::parser("status")                      ,&Patch::_status);
    patch(Uri::parser("meta/{source?}")              ,&Patch::_meta);
    //addRoute(Uri::parser("scan/{file?}")              ,&Patch::_scan);
    //addRoute(Uri::parser("scan")                        ,"_scan"                ,PATCH);

//    addRoute(Uri::parser("connect/{mac?}/{name?}")      ,"_connectSource"       ,PATCH);
//    addRoute(Uri::parser("connect/{mac?}")              ,"_connect"             ,PATCH);
    patch(Uri::parser("disconnect/{type}/{source}")  ,&Patch::_disconnect);
//    addRoute(Uri::parser("remove/{source?}")            ,"_removeSource"        ,PATCH);

    patch(Uri::parser("pipe/{name?}")                ,&Patch::_openPipe);

    patch(Uri::parser("config/{file?}")              ,&Patch::_config);

    patch(Uri::parser("shutdown")                    ,&Patch::_shutdown);
    patch(Uri::parser("restart")                     ,&Patch::_restart);

    get(Uri::parser("{source?}?c-")                  ,&Get::_get);

    catchall(Uri::parser("")                         ,&Catch::catchall);
}

template <>
std::vector<Route<Get>> const& Server::getRoutes<Get>()const{ return get_routes; }
template <>
std::vector<Route<Head>> const& Server::getRoutes<Head>()const{ return head_routes; }
template <>
std::vector<Route<Options>> const& Server::getRoutes<Options>()const{ return options_routes; }
template <>
std::vector<Route<Post>> const& Server::getRoutes<Post>()const{ return post_routes; }
template <>
std::vector<Route<Patch>> const& Server::getRoutes<Patch>()const{ return patch_routes; }
template <>
std::vector<Route<Catch>> const& Server::getRoutes<Catch>()const{ return catch_routes; }

/**
 * @brief Server::start : start the server
 */
void Server::start()
{
    Log() << "Started Server";
    std::lock_guard<std::mutex> l(drumlin::iapp->m_critical_section);
    defineRoutes();
    ServerBase::start();
}

void Server::stop()
{
    Log() << "Stopped Server";
    ServerBase::stop();
    Debug() << "Removing sources...";
    Sources::sources.removeAll();
    ApplicationBase::stop();
}

void Server::writeLog()
{
    std::lock_guard<std::recursive_mutex> l(mutex);
    ofstream logfile;
    logfile.open("./Server.log",ios_base::out|ios_base::app);
    if(logfile.is_open()){
        for(string const& str : log){
            logfile << str << endl;
        }
        logfile.flush();
        logfile.close();
        log.clear();
    }
}

/**
 * @brief Server::getStatus : return a list.join("\n") of running threads
 * @return const char*
 */
void Server::getStatus(json::value *status)const
{
    ApplicationBase::getStatus(status);
    {
        std::lock_guard<std::mutex> l(const_cast<std::mutex&>(m_critical_section)); //Server inherits Application<T>

        json::value requests(json::empty_array);
        json::array_t &threads(status->get_object().at("threads").get_array());

        for(json::array_t::iterator::value_type &thread : threads){
            if(std::string::npos != thread.get_object().at("task").get_string().find("Request")){
                requests.get_array().push_back(json::value(thread));
            }
        }

        if(distance(log.begin(),log.end())){
            json::value _log(json::empty_array);
            json::array_t &logs(_log.get_array());
            for(string const& str : log){
                logs.push_back(str);
            }
            status->get_object().insert({"log",_log});
            const_cast<Pleg::Server*>(this)->writeLog();
        }

        status->get_object().insert({"requests",requests});
    }
}

} // namespace Pleg
