#ifndef ROUTE_H
#define ROUTE_H

#include <numeric>
using namespace std;
#include "drumlin/socket.h"
using namespace drumlin;
#include "uri.h"

namespace Pleg {

template <class Response>
struct Route {
public:
    UriParseFunc parse_func;
    typedef void _Return;
    typedef void _Param;
    typedef _Return (Response::*func)(_Param);
    func response_func;
    Route():parse_func(nullptr),response_func(nullptr){}
    Route(const UriParseFunc &&_parse_func,func response_func)
        :parse_func(std::move(_parse_func)),response_func(response_func){}
    std::string toString(bool detail = false)const{
        string str(::metaEnum<verbs_type>().toString(Response::method()));
        str += " " + parse_func.pattern;
        if(detail){
            str += std::accumulate(parse_func.params.begin(), parse_func.params.end(), string(),
                [](const string& a, const string& b) -> string {
                    return a + (a.length() > 0 ? "," : "") + b;
                }
            );
        }
        return str;
    }
    void operator()(Response *response)const
    {
        (response->*response_func)();
    }
};

template <class Response>
Route<Response> make_route(const UriParseFunc &&_parse_func,void (Response::*func)())
{
    return Route<Response>(std::move(_parse_func),func);
}

} // namespace Pleg

#endif // ROUTE_H
