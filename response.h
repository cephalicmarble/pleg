#ifndef RESPONSE_H
#define RESPONSE_H

#include <memory>
using namespace std;
#include "object.h"
#include "uri.h"
#include "relevance.h"
#include "buffer.h"
#include "socket.h"
#include "mutexcall.h"
#include "jsonconfig.h"
#include "request.h"
#include "thread.h"
using namespace drumlin;

namespace Pleg {

class ResponseWriter;

/**
 * @brief The Response class : abstract base
 */
class Response :
    public Object,
    public StatusReporter
{
public:
    string getStatusCode()const { return statusCode; }
    /**
     * @brief Response : only constructor
     * @param _req Request*
     */
    Response(Request *_req) : Object(),req(_req){}
    /**
     * @brief getRequest
     * @return Request*
     */
    Request *getRequest()const{ return req; }
    /**
     * @brief getWorker
     * @return ThreadWorker*
     */
    ThreadWorker *getWorker()const{ return worker; }
    /**
     * @brief setWriter
     * @param _writer ResponseWriter*
     * @return Response*
     */
    Response *setWriter(ResponseWriter *_writer){ writer = _writer; return this; }
    /**
     * @brief getWriter
     * @return ResponseWriter*
     */
    ResponseWriter *getWriter(){ return writer; }
    virtual void service()=0;
    virtual ~Response();
    virtual void writeResponse();
    virtual void getStatus(json::value *status)const;
public:
    vector<string> headers;
protected:
    Request *req;
    ThreadWorker *worker;
    ResponseWriter *writer = nullptr;
    string statusCode = "403 Forbidden";
};

class Options :
    public Response
{
public:
    Options(Request *req);
    void service();
};

class Head :
    public Response
{
public:
    Head(Request *req);
    void service();
};

/**
 * @brief The Get class : HTTP GET
 */
class Get :
    public Response,
    public Continuer<Buffers::findRelevant_t>
{
public:
    Get(Request *req);
    virtual void service();
    virtual ~Get() {}
    virtual void accept(Buffers::findRelevant_t &,Buffers::buffer_vec_type buffers);

    void _get();
    void _dir();
    gint64 readRange(ifstream &device,string mime,gint64 completeLength,string range);
    void _file();
    void _lsof();
    void catchall(){ _file(); }
private:
    Buffers::buffer_vec_type data;
    string boundary = "--";
};

/**
 * @brief The Post class : HTTP POST
 */
class Post : public Response {
    Q_OBJECT
public:
    Post(Request *req);
    virtual void service();
    virtual ~Post() {}

    void _touch();
    void _mkdir();
    void makeWriterFile();
    void stopWriterFile();
    void teeSourcePort();
};

class Thread;

/**
 * @brief The Patch class : HTTP PATCH
 */
class Patch : public Response {
    Q_OBJECT

public:
    Patch(Request *req);
    virtual void service();
    virtual ~Patch() {}
public:
    void _routes();
    void _devices();
    void _status();
    void _meta();
//    void _connect();
//    void _connectSource();
//    void _disconnect();
//    void _scan();
    void _openPipe();
    void _config();
    void _shutdown();
    void _restart();
    void _removeSource();
public slots:
    void timedOut();
    void writeToSocket();
};

class Dummy : public Response
{
public:
    Dummy(Request *req):Response(req){}
    virtual void service();
    void catchall();
};

} // namespace drumlin

#endif // RESPONSE_H
