#ifndef RESPONSE_H
#define RESPONSE_H

#include <QObject>
#include <QMutex>
#include <memory>
#include "uri.h"
#include "relevance.h"
#include "buffer.h"
#include "socket.h"
#include "mutexcall.h"
#include "jsonconfig.h"
#include "request.h"
#include "thread.h"

class QSResponseWriter;

/**
 * @brief The QSResponse class : abstract base
 */
class QSResponse :
    public QObject,
    public QSStatusReporter
{
    Q_OBJECT

protected:
    QSRequest *req;
    QSThreadWorker *worker;
    QSResponseWriter *writer = nullptr;
    QString statusCode = "403 Forbidden";
public:
    QStringList headers;
    QString getStatusCode()const { return statusCode; }
    /**
     * @brief QSResponse : only constructor
     * @param _req QSRequest*
     */
    QSResponse(QSRequest *_req) : QObject(),req(_req){}
    /**
     * @brief getRequest
     * @return QSRequest*
     */
    QSRequest *getRequest()const{ return req; }
    /**
     * @brief getWorker
     * @return QSThreadWorker*
     */
    QSThreadWorker *getWorker()const{ return worker; }
    /**
     * @brief setWriter
     * @param _writer QSResponseWriter*
     * @return QSResponse*
     */
    QSResponse *setWriter(QSResponseWriter *_writer){ writer = _writer; return this; }
    /**
     * @brief getWriter
     * @return QSResponseWriter*
     */
    QSResponseWriter *getWriter(){ return writer; }
    virtual void service()=0;
    virtual ~QSResponse();
    virtual void writeResponse();
    virtual void getStatus(json::value *status)const;
};

class QSOptions :
    public QSResponse
{
public:
    QSOptions(QSRequest *req);
    void service();
};

class QSHead :
    public QSResponse
{
public:
    QSHead(QSRequest *req);
    void service();
};

/**
 * @brief The QSGet class : HTTP GET
 */
class QSGet :
    public QSResponse,
    public Continuer<Buffers::findRelevant_t>
{
    Q_OBJECT

    Buffers::buffer_vec_type data;
    QString boundary = "--";
public:
    QSGet(QSRequest *req);
    virtual void service();
    virtual ~QSGet() {}
    virtual void accept(Buffers::findRelevant_t &,Buffers::buffer_vec_type buffers);

    Q_INVOKABLE void _get();
    Q_INVOKABLE void _dir();
    qint64 readRange(QIODevice *device,QString mime,qint64 completeLength,QString range);
    Q_INVOKABLE void _file();
    Q_INVOKABLE void _lsof();
    Q_INVOKABLE void catchall(){ _file(); }
};

/**
 * @brief The QSPost class : HTTP POST
 */
class QSPost : public QSResponse {
    Q_OBJECT
public:
    QSPost(QSRequest *req);
    virtual void service();
    virtual ~QSPost() {}

    Q_INVOKABLE void _touch();
    Q_INVOKABLE void _mkdir();
    Q_INVOKABLE void makeWriterFile();
    Q_INVOKABLE void stopWriterFile();
#if defined(GSTREAMER) || defined(QTGSTREAMER)
    Q_INVOKABLE void teeSourcePort();
#endif
};

class QSThread;

/**
 * @brief The QSPatch class : HTTP PATCH
 */
class QSPatch : public QSResponse {
    Q_OBJECT

public:
    QSPatch(QSRequest *req);
    virtual void service();
    virtual ~QSPatch() {}
public:
    Q_INVOKABLE void _routes();
    Q_INVOKABLE void _devices();
    Q_INVOKABLE void _status();
    Q_INVOKABLE void _meta();
    Q_INVOKABLE void _connect();
    Q_INVOKABLE void _connectSource();
    Q_INVOKABLE void _disconnect();
    Q_INVOKABLE void _scan();
    Q_INVOKABLE void _openPipe();
    Q_INVOKABLE void _config();
    Q_INVOKABLE void _shutdown();
    Q_INVOKABLE void _restart();
    Q_INVOKABLE void _removeSource();
public slots:
    void timedOut();
    void writeToSocket();
};

class QSDummy : public QSResponse
{
    Q_OBJECT

public:
    QSDummy(QSRequest *req):QSResponse(req){}
    virtual void service();
    Q_INVOKABLE void catchall();
};

#endif // RESPONSE_H
