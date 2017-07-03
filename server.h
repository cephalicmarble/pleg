#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <list>
#include <iostream>
using namespace std;
#include <boost/asio.hpp>
#include <boost/thread/recursive_mutex.hpp>
using namespace boost;
#include "object.h"
#include "socket.h"
#include "thread.h"
#include "signalhandler.h"
#include "application.h"
#include "connection.h"
using namespace drumlin;
#include "relevance.h"
#include "uri.h"

namespace drumlin {

class Request;
class Response;

}

namespace Pleg {

class Bluetooth;
namespace GStreamer {
    class GStreamer;
}

#define verbs (\
    NONE,\
    HEAD,\
    GET,\
    POST,\
    PATCH,\
    CATCH,\
    OPTIONS,\
)
ENUM(verbs_type,verbs)


struct Route {
public:
    typedef verbs_type verbs_type;
    verbs_type method;
    UriParseFunc parse_func;
    const char*response_func;
    Route():method(NONE),parse_func(nullptr),response_func(nullptr){}
    Route(int _method,const UriParseFunc &&_parse_func,const char*_response_func)
        :method(_method),parse_func(std::move(_parse_func)),response_func(_response_func){}
    std::string toStdString(bool detail = false)const{
        tring str(metaEnum<verbs_type>().toString(method));
        str += " " + parse_func.pattern;
        if(detail){
            str += std::accumulate(parse_func.params.begin(), parse_func.params.end(), tring(),
                [](const tring& a, const tring& b) -> tring {
                    return a + (a.length() > 0 ? "," : "") + b;
                }
            );
        }
        return str.toStdString();
    }
};

/**
 * @brief The Server class : HTTP socket server
 */
class Server :
    public Application<PlegApplication>,
    public StatusReporter,
    public AsioServer<Request>
{
public:
    typedef Application<PlegApplication> ApplicationBase;
    typedef AsioServer<Request> ServerBase;
    /**
     * @brief routes_type : vector of UriParseFunc instances
     */
    typedef std::vector<Route> routes_type;

    void defineRoutes();

    /**
     * @brief addRoute
     * @param func UriParseFunc Uri::parser("{pattern?}/{pattern?}")
     */
    void addRoute(const UriParseFunc &&func,const char*response_func,int method = Route::GET){
        routes.push_back({method,std::move(func),response_func});
    }
    routes_type const& getRoutes()const{ return routes; }
    void writeLog()const;
    explicit Server(int &argc,char *argv[],int port);
    ~Server();
    typedef std::pair<Relevance,Server::routes_type::iterator> route_return_type;
    route_return_type select_route(Request *request);

    void getStatus(json::value *status)const;

    operator const char*()const;
    friend ostream &operator<<(ostream &stream,const Server &rel);
    void stop();
    void start();
private:
    vector<string> &getLog(){ return log; }
    friend class Pleg::log;
public:
    static recursive_mutex mutex;
    bool closed = false;
private:
    routes_type routes;
    vector<string> log;
};

#endif // SERVER_H
