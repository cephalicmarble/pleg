#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <list>
#include <iostream>
#include <numeric>
#include <mutex>
using namespace std;
#include <boost/asio.hpp>
#include <boost/functional.hpp>
using namespace boost;
#include "drumlin/object.h"
#include "drumlin/socket.h"
#include "drumlin/thread.h"
#include "drumlin/signalhandler.h"
#include "drumlin/application.h"
#include "drumlin/connection.h"
using namespace drumlin;
#include "relevance.h"
#include "uri.h"
#include "route.h"
#include "request.h"
#include "response.h"
#include "log.h"
#include "plegapp.h"

namespace Pleg {

class Response;
//class Bluetooth;
namespace GStreamer {
    class GStreamer;
}

/**
 * @brief The Server class : HTTP socket server
 */
class Server :
    public Application<PlegApplication>,
    public AsioServer<Server,Request>
{
public:
    typedef Application<PlegApplication> ApplicationBase;
    typedef AsioServer<Server,Request> ServerBase;
    /**
     * @brief routes_type : vector of UriParseFunc instances
     */
    typedef std::vector<Route<Get>> get_routes_type;
    typedef std::vector<Route<Head>> head_routes_type;
    typedef std::vector<Route<Options>> options_routes_type;
    typedef std::vector<Route<Post>> post_routes_type;
    typedef std::vector<Route<Patch>> patch_routes_type;
    typedef std::vector<Route<Catch>> catch_routes_type;

    void defineRoutes();

    /**
     * @brief addRoute
     * @param func UriParseFunc Uri::parser("{pattern?}/{pattern?}")
     */
    void get(const UriParseFunc &&func,void (Get::*response_func)()){
        get_routes.push_back(make_route(std::move(func),response_func));
    }
    void head(const UriParseFunc &&func,void (Head::*response_func)()){
        head_routes.push_back(make_route(std::move(func),response_func));
    }
    void options(const UriParseFunc &&func,void (Options::*response_func)()){
        options_routes.push_back(make_route(std::move(func),response_func));
    }
    void post(const UriParseFunc &&func,void (Post::*response_func)()){
        post_routes.push_back(make_route(std::move(func),response_func));
    }
    void patch(const UriParseFunc &&func,void (Patch::*response_func)()){
        patch_routes.push_back(make_route(std::move(func),response_func));
    }
    void catchall(const UriParseFunc &&func,void (Catch::*response_func)()){
        catch_routes.push_back(make_route(std::move(func),response_func));
    }
    template <class Response>
    std::vector<Route<Response>> const& getRoutes()const;
    void writeLog();
    Server(int argc,char **argv,string address,int port);
    ~Server();
    /**
     * @brief Server::select_route : find a route
     * @param request
     * @return
     */
    template <class ResponseClass>
    bool select_route(Request *request,ResponseClass *response,Relevance *relevance)
    {
        typedef std::vector<Route<ResponseClass>> routes_type;
        std::lock_guard<std::recursive_mutex> l(mutex);
        routes_type const& routes(getRoutes<ResponseClass>());
        auto it = std::find_if(routes.begin(),routes.end(),[relevance,request](auto & route){
            *relevance = route.parse_func(request->getUrl());
            if(relevance->toBool()){
                return true;
            }
            return false;
        });
        if(it == routes.end()){
            Debug() << "Irrelevant HTTP";
            return false;
        }
        Debug() << it->parse_func.pattern;
        (*it)(const_cast<ResponseClass*>(response));
        return true;
    }

    void getStatus(json::value *status)const;

    operator const char*()const;
    friend logger &operator<<(logger &stream,const Server &rel);
    void stop();
    void start();
private:
    vector<string> &getLog(){ return log; }
    friend class Pleg::Log;
public:
    std::recursive_mutex mutex;
    bool closed = false;
private:
    get_routes_type get_routes;
    head_routes_type head_routes;
    options_routes_type options_routes;
    post_routes_type post_routes;
    patch_routes_type patch_routes;
    catch_routes_type catch_routes;
    vector<string> log;
};

template <>
std::vector<Route<Get>> const& Server::getRoutes<Get>()const;
template <>
std::vector<Route<Head>> const& Server::getRoutes<Head>()const;
template <>
std::vector<Route<Options>> const& Server::getRoutes<Options>()const;
template <>
std::vector<Route<Post>> const& Server::getRoutes<Post>()const;
template <>
std::vector<Route<Patch>> const& Server::getRoutes<Patch>()const;
template <>
std::vector<Route<Catch>> const& Server::getRoutes<Catch>()const;

} // namespace Pleg

#endif // SERVER_H
