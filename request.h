#ifndef REQUEST_H
#define REQUEST_H

#include <string>
#include <list>
#include <map>
#include <memory>
using namespace std;
#include "object.h"
#include "thread.h"
#include "server.h"
#include "uri.h"
#include "relevance.h"
#include "socket.h"
#include "thread.h"
#include "connection.h"
#include "byte_array.h"
using namespace drumlin;

namespace Pleg {

class Response;

/**
 * @brief The Request class : Runnable HTTP request handler
 */
class Request :
    public ThreadWorker
    ,public SocketHandler
    ,public Connection<Request>
{
public:
    typedef Connection<Request> connection_type;
    typedef typename connection_type::socket_type Socket;
    typedef map<string,string> headers_type;
    typedef Route::verbs_type verbs_type;
    Request(Server *parent,Thread *_thread);
    virtual ~Request();

    Socket *getSocket();
    /**
     * @brief getHeader : get HTTP header by name or blank string
     * @param name tring const&
     * @return tring
     */
    string getHeader(const string &name)
    {
        if(headers.contains(name))
            return *headers.find(name);
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
    virtual void completing(Socket *,qint64 bytes);

    virtual void writeToStream(std::ostream &stream)const;
    virtual void getStatus(json::value *status)const;

    virtual void shutdown();
    virtual bool event(QEvent *event);

    friend class Patch;
    virtual void run(QObject *obj,Event *event);
    virtual void connection_start();

public:
    headers_type headers;
    bool delayed = false;
protected:
    verbs_type verb = verbs_type::NONE;
    string url;
    bool moreHeaders = true;
    byte_array body;
    Route *route = nullptr;
    Relevance relevance;
    Response *response = nullptr;
    Response *createResponse();
private:
    Server *server;
    Socket *socket;
    byte_array data;
    gintptr socketDescriptor = 0;
};

#endif // REQUEST_H
