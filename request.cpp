#include "request.h"
#include <pleg.h>
using namespace Pleg;
#include <algorithm>
#include <iterator>
#include <memory>
#include <sstream>
using namespace std;
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/tokenizer.hpp>
using namespace boost;
#include "thread.h"
#include "application.h"
#include "exception.h"
#include "response.h"
#include "factory.h"
#include "socket.h"
#include "server.h"
using namespace drumlin;

namespace Pleg {

/**
 * @brief Request::Request : only constructor
 * @param parent Server*
 */
Request::Request(Pleg::Server *_server)
    : ThreadWorker(ThreadType_http,(Object*)_server),SocketHandler(),connection_type(this)
{
    server = _server;
}

/**
 * @brief Request::~Request
 */
Request::~Request()
{
    if(nullptr != response)
        delete response;
}

/**
 * @brief Request::run : the thread function
 */
void Request::work(Object *,Event *)
{
    getSocketRef()
        .setTag(getThread())
        .setHandler(this)
        .setConnection(this)
        .reading()
    ;
}

void Request::connection_start()
{
    lock_guard<recursive_mutex> l(critical_section);
    thread = new Thread(string("Request:")+posix_time::to_iso_string(posix_time::microsec_clock::universal_time()),this);
    Debug() << "Starting thread " << this << *thread;
    app->addThread(thread);
}

void Request::error(boost::system::error_code ec)
{
    Critical() << ec.message();
    close();
}

void Request::close()
{
    signalTermination();
}

/**
 * @brief Request::readyProcessImpl : process HTTP request packet if not seen already
 * @return bool
 */
bool Request::readyProcess(Socket*)
{
    return verb == verbs_type::NONE || moreHeaders;
}

/**
 * @brief Request::processTransmissionImpl : do HTTP protocol parse
 * @return
 */
bool Request::processTransmission(Socket *socket)
{
    vector<string> lines;
    byte_array bytes(socket->peekData(SocketFlushBehaviours::CoalesceAndFlush));
    string str(bytes.string());
    string::size_type pos(string::npos);
    while((pos=str.find("\r\n"))!=string::npos)str.replace(pos,2,"¬");
    algorithm::split(lines,str,algorithm::is_any_of("¬"),algorithm::token_compress_on);
    for(string &line : lines){
        if(verb == verbs_type::NONE) {
            vector<string> parts;
            algorithm::split(parts,line,algorithm::is_any_of(" "),algorithm::token_compress_on);
            if(distance(parts.begin(),parts.end())<3){
                Critical() << "Bad HTTP";
                return false;
            }
            algorithm::trim(parts[2]);
            if(parts[2].compare("HTTP/1.1") && parts[2].compare("HTTP/1.0")){
                Critical() << "Outmoded HTTP";
                return false;
            }
            string verbp(parts[0]);
            bool ok = false;
            verb = (verbs_type)metaEnum<verbs_type>().toEnum(verbp,&ok);
            if(!ok){
                Critical() << "Bad HTTP verb";
                return false;
            }
            algorithm::trim(parts[1]);
            url = parts[1];
        }else{
            if(moreHeaders) {
                if(!line.length()){
                    moreHeaders = false;
                }else{
                    vector<string> parts;
                    algorithm::split(parts,line,algorithm::is_any_of(":"),algorithm::token_compress_on);
                    if(distance(parts.begin(),parts.end())>1){
                        algorithm::trim(parts[0]);
                        algorithm::trim(parts[1]);
                        headers.insert({parts[0],parts[1]});
                    }
                }
            }else if(verb == verbs_type::POST){
                body.append(line);
            }
        }
        //Debug() << line;
    }
    if(verb != verbs_type::NONE && !moreHeaders){
        if(!getHeader("X-Method").compare("PATCH")){
            verb = verbs_type::PATCH;
        }
        Debug() << metaEnum<verbs_type>().toString(verb);
        for(headers_type::value_type & hdr : headers){
            Debug().getStream() << hdr.first << ":" << hdr.second;
        }
        return true;
    }
    return false;
}

/**
 * @brief Request::replyImpl : the heart of the HTTP service
 * HEAD : return quickly
 * PATCH : unhandled - server control messages
 * GET, POST : factory-create response and service request, complete socket
 */
bool Request::reply(Socket *) {
    response = Factory::Response::create(this);
    response->service();
    switch(verb){
    case GET:
        getServer()->select_route(this,dynamic_cast<Get*>(response),&relevance);
        break;
    case POST:
        getServer()->select_route(this,dynamic_cast<Post*>(response),&relevance);
        break;
    case PATCH:
        getServer()->select_route(this,dynamic_cast<Patch*>(response),&relevance);
        break;
    case HEAD:
        getServer()->select_route(this,dynamic_cast<Head*>(response),&relevance);
        break;
    case OPTIONS:
        getServer()->select_route(this,dynamic_cast<Options*>(response),&relevance);
        break;
    case CATCH:
        getServer()->select_route(this,dynamic_cast<Catch*>(response),&relevance);
        break;
    default:
        break;
    }
    if(!delayed){
        end();
    }
    return true;
}

/**
 * @brief Request::completingImpl : write a debug string
 * @param bytes quint32 number of bytes written
 */
void Request::completing(Socket *socket)
{
    if(socket->writeQueueLength())
        return;
    close();
}

void Request::error(Socket *,boost::system::error_code &ec)
{
    Critical() << ec.message();
    close();
}

/**
 * @brief Request::end : end the request
 */
void Request::end()
{
    Debug() << "finished";
    string_list msg;
    msg << metaEnum<verbs_type>().toString(verb) << url << "finished.";
    response->headers << "Access-Control-Allow-Origin: *";
    string headers(algorithm::join(response->headers,"\r\n"));
    std::stringstream ss;
    ss << "HTTP/1.1 " << response->getStatusCode() << "\r\n";
    ss << headers << "\r\n";
    if(string::npos == headers.find("Content-Length")){
        ss << "Content-Length: " << socket().bytesToWrite() << "\r\n";
    }
    ss << "\r\n";
    socket().write(ss.str(),true);
    socket().writing();
}

void Request::shutdown()
{
    close();
}

/**
 * @brief Request::writeToStream : for the status update
 * @param stream std::ostream
 */
void Request::writeToStream(std::ostream &stream)const
{
    string_list list;
    list << metaEnum<verbs_type>().toString(verb) << url << "HTTP/1.1";
    stream << algorithm::join(list," ");
}

void Request::getStatus(json::value *status)const{
    if(response)
        response->getStatus(status);
}

/**
 * @brief Request::eventFilter : deal with bluetooth events
 * @param obj QObject*
 * @param event QEvent*
 * @return bool
 */
bool Request::event(Event *pevent)
{
    quietDebug() << this << __func__ << metaEnum<Event::Type>().toString(pevent->type());
    if((guint32)pevent->type() > (guint32)Event_first
            && (guint32)pevent->type() < (guint32)Event_last){
        switch(pevent->type()){
        case Event::Type::GstStreamPort:
        {
            getSocketRef().write("Opened port:"+getRelevanceRef().arguments.at("port"));
            end();
            break;
        }
        case Event::Type::GstStreamFile:
        {
            getSocketRef().write("Writing file:"+getRelevanceRef().arguments.at("filepath"));
            Patch patch(this);
            patch._status();
            end();
            break;
        }
        default:
            Debug() << metaEnum<verbs_type>().toString(verb) << __func__ <<  "unimplemented";
            return false;
        }
        return true;
    }
    return false;
}

} // namespace Pleg
