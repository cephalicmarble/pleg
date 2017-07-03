#include "request.h"
#include <pleg.h>
using namespace Pleg;
#include <algorithm>
#include <iterator>
#include <memory>
#include <sstream>
using namespace std;
#include <boost/date_time/posix_time/posix_time.hpp>
using namespace boost;
#include "application.h"
#include "exception.h"
#include "response.h"
#include "factory.h"
#include "socket.h"
using namespace drumlin;

namespace Pleg {

/**
 * @brief Request::Request : only constructor
 * @param parent Server*
 */
Request::Request(Server *parent)
    : ThreadWorker(parent),SocketHandler(),connection_type()
{
    type = http;
    server = parent;
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
void Request::run(Object *obj,Event *event)
{
    socket->setTag(getThread());
    while(socket->async_receive(

TODO receive asynchronously
}

void Request::connection_start()
{
    qDebug() << "Starting thread " << req << thread;
    thread = new Thread(string("Request:")+posix_time::to_iso_string(posix_time::microsec_clock::universal_time()));
    thread->setWorker(this);
    app->addThread(thread,true);
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
 * @brief Request::completingImpl : write a debug string
 * @param bytes quint32 number of bytes written
 */
void Request::completing(Socket *,qint64 bytes)
{
    qDebug() << socket << "wrote" << bytes << "bytes.";
//    QThread::sleep(1);
    getThread()->quit();
}

/**
 * @brief Request::processTransmissionImpl : do HTTP protocol parse
 * @return
 */
bool Request::processTransmission(Socket *)
{
    QList<tring> lines = tring(socket->peekData(SocketFlushBehaviours::CoalesceAndFlush)).split("\r\n");
    for(tring &line : lines){
        if(verb == verbs_type::NONE) {
            QList<tring> parts = line.split(' ',tring::SplitBehavior::SkipEmptyParts);
            if(parts.length()<3 || (parts[2].trimmed().compare("HTTP/1.1") && parts[2].trimmed().compare("HTTP/1.0"))){
                qCritical() << "Bad HTTP";
                socket->close();
                return false;
            }
            tring verbp(parts[0]);
            bool ok = false;
            verb = (verbs_type)QMetaEnum::fromType<verbs_type>().keyToValue(verbp.toStdString().c_str(),&ok);
            if(!ok){
                qCritical() << "Bad HTTP verb";
                socket->close();
                return false;
            }
            url = parts[1].trimmed();
        }else{
            if(moreHeaders) {
                if(!line.length()){
                    moreHeaders = false;
                }else{
                    QList<tring> parts = line.split(':');
                    if(parts.length()>1)
                        headers.insert(parts[0].trimmed(),parts[1].trimmed());
                }
            }else if(verb == verbs_type::POST){
                body.append(line);
            }
        }
        qDebug() << line;
    }
    if(verb != verbs_type::NONE && !moreHeaders){
        if(!getHeader("X-Method").compare("PATCH")){
            verb = verbs_type::PATCH;
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
bool Request::replyImpl(Socket *) {
    if(verb != verbs_type::HEAD && verb != verbs_type::OPTIONS){
        auto selected = server->select_route(this);
        relevance = selected.first;
        route = &*selected.second;
        if(route->method==Route::NONE || !relevance.toBool()){
            verb = verbs_type::CATCH;
        }
    }
    response = Factory::Response::create(this);
    response->service();
    if(route)
        response->metaObject()->invokeMethod(response,route->response_func,Qt::DirectConnection);
    if(!delayed){
        end();
    }
    return true;
}

/**
 * @brief Request::end : end the request
 */
void Request::end()
{
    qDebug() << "finished";
    tringList msg;
    msg << QMetaEnum::fromType<verbs_type>().valueToKey(verb) << url << "finished.";
    response->headers << "Access-Control-Allow-Origin: *";
    tring headers(response->headers.join("\r\n"));
    std::stringstream ss;
    ss << "HTTP/1.1 " << response->getStatusCode() << "\r\n";
    ss << headers.toStdString() << "\r\n";
    if(!~headers.indexOf("Content-Length")){
        ss << "Content-Length: " << socket->bytesToWrite() << "\r\n";
    }
    ss << "\r\n";
    socket->write(ss.str().c_str(),true);
    if(socket->state()==Socket::ConnectedState)
        close();
}

void Request::shutdown()
{
    if(socket && socket->state()==Socket::ConnectedState)
        end();
    signalTermination();
}

/**
 * @brief Request::getSocket
 * @return Socket*
 */
Socket *Request::getSocket()
{
    return socket;
}

/**
 * @brief Request::writeToStream : for the status update
 * @param stream std::ostream
 */
void Request::writeToStream(std::ostream &stream)const
{
    tringList list;
    list << QMetaEnum::fromType<verbs_type>().valueToKey(verb) << url << "HTTP/1.1";
    stream << list.join(" ").toStdString();
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
bool Request::event(QEvent *event)
{
    Event *pevent = dynamic_cast<Event*>(event);
    if(!pevent){
        quietDebug() << this << __func__ << "Unimplemented" << event->type();
        return false;
    }
    quietDebug() << this << __func__ << pevent->type();
    if((quint32)pevent->type() > (quint32)Event::Type::first
            && (quint32)pevent->type() < (quint32)Event::Type::last){
        switch(pevent->type()){
        case Event::Type::GstStreamPort:
        {
            getSocket()->write("Opened port:"+getRelevanceRef().arguments.at("port"));
            signalTermination();
            break;
        }
        case Event::Type::GstStreamFile:
        {
            getSocket()->write("Writing file:"+getRelevanceRef().arguments.at("filepath"));
            Patch patch(this);
            patch._status();
            signalTermination();
            break;
        }
        default:
            qDebug() << type << __func__ <<  "unimplemented";
            return false;
        }
        return true;
    }
    return false;
}
