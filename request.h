#ifndef REQUEST_H
#define REQUEST_H

#include <string>
#include <list>
#include <map>
#include <memory>
using namespace std;
#include "object.h"
#include "thread.h"
#include "uri.h"
#include "relevance.h"
#include "socket.h"
#include "thread.h"
#include "connection.h"
#include "byte_array.h"
#include "route.h"
using namespace drumlin;

namespace Pleg {

class Response;
class Server;

/**
 * @brief The Request class : Runnable HTTP request handler
 */
class Request :
    public ThreadWorker
    ,public SocketHandler<asio::ip::tcp>
    ,public Connection<asio::ip::tcp>
{
public:
    typedef Connection<asio::ip::tcp> connection_type;
    typedef typename connection_type::socket_type Socket;
    typedef map<string,string> headers_type;
    Request(Pleg::Server *_server);
    virtual ~Request();

    socket_type &getSocketRef()
    {
        return socket();
    }
    /**
     * @brief getHeader : get HTTP header by name or blank string
     * @param name tring const&
     * @return tring
     */
    string getHeader(const string &name)
    {
        auto iter(headers.find(name));
        if(headers.end()!=iter)
            return iter->second;
        return "";
    }
    /**
     * @brief getVerb : gets the parsed verb
     * @return verbs_type
     */
    verbs_type getVerb(){ return verb; }
    /**
     * @brief getUrl : gets the url
     * @return tring
     */
    string getUrl(){ return url; }
    /**
     * @brief getRelevance : gets the relevance of the URL
     * @return Relevance
     */
    Relevance *getRelevance(){ return &relevance; }
    /**
     * @brief getRelevanceRef : gets const reference to relevance
     * @return Relevance const&
     */
    const Relevance &getRelevanceRef()const{ return relevance; }
    /**
     * @brief getServer
     * @return Server*
     */
    Server *getServer(){ return server; }
    void end();

    virtual bool processTransmission(Socket*);
    virtual bool readyProcess(Socket*);
    virtual bool reply(Socket*);
    virtual void completing(Socket *,gint64 bytes);
    virtual bool receivePacket(Socket *){return false;}
    virtual void sort(Socket *,drumlin::buffers_type &){}
    virtual void disconnected(Socket *){}

    virtual void writeToStream(std::ostream &stream)const;
    virtual void getStatus(json::value *status)const;

    virtual void shutdown();
    virtual bool event(Event *event);

    friend class Patch;
    virtual void run(Object *obj,Event *event);
    virtual void connection_start();

public:
    headers_type headers;
    bool delayed = false;
protected:
    verbs_type verb = verbs_type::NONE;
    string url;
    bool moreHeaders = true;
    byte_array body;
    Relevance relevance;
    Response *response = nullptr;
    Response *createResponse();
private:
    Server *server;
    byte_array data;
};

} // namespace Pleg

#endif // REQUEST_H
