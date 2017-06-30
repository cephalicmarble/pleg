#ifndef QSSERVER_H
#define QSSERVER_H

#include "object.h"

#include <QTcpServer>
#include <QMutex>
#include <vector>
#include <list>
#include <iostream>
#include "uri.h"
#include "relevance.h"
#include "socket.h"
#include "thread.h"
#include "signalhandler.h"
#include "application.h"

class QSRequest;
class QSResponse;
class QSBluetooth;
namespace GStreamer {
    class QSGStreamer;
}

struct QSRoute {
    Q_GADGET
public:
    enum verbs_type {
        NONE    = 0,
        HEAD    = 1,
        GET     = 2,
        POST    = 4,
        PATCH   = 8,
        CATCH   = 16,
        OPTIONS = 32,
        CONTROL = PATCH | GET,
    };
    Q_ENUM(verbs_type)
    int method;
    QSUriParseFunc parse_func;
    const char*response_func;
    QSRoute():method(NONE),parse_func(nullptr),response_func(nullptr){}
    QSRoute(int _method,const QSUriParseFunc &&_parse_func,const char*_response_func)
        :method(_method),parse_func(std::move(_parse_func)),response_func(_response_func){}
    std::string toStdString(bool detail = false)const{
        QString str(QMetaEnum::fromType<verbs_type>().valueToKey(method));
        str += " " + parse_func.pattern;
        if(detail){
            str += std::accumulate(parse_func.params.begin(), parse_func.params.end(), QString(),
                [](const QString& a, const QString& b) -> QString {
                    return a + (a.length() > 0 ? "," : "") + b;
                }
            );
        }
        return str.toStdString();
    }
};

class QSTcpServer : public QTcpServer
{
    Q_OBJECT
signals:
    void incomingConnection(qintptr socketDescriptor);
};

/**
 * @brief The QSServer class : HTTP socket server
 */
class QSServer :
    public QSApplication<QCoreApplication>,
    public QSStatusReporter
{
    Q_OBJECT
    /**
     * @brief routes_type : vector of QSUriParseFunc instances
     */
    QSTcpServer *server;
    QStringList log;
public:
    typedef QSApplication<QCoreApplication> Base;
    typedef std::vector<QSRoute> routes_type;
private:
    routes_type routes;

    void defineRoutes();

public:
    static QMutex mutex;
    bool closed = false;
    /**
     * @brief addRoute
     * @param func QSUriParseFunc Uri::parser("{pattern?}/{pattern?}")
     */
    void addRoute(const QSUriParseFunc &&func,const char*response_func,int method = QSRoute::GET){
        routes.push_back({method,std::move(func),response_func});
    }
    routes_type const& getRoutes()const{ return routes; }
    QStringList &getLog(){ return log; }
    void writeLog()const;
    explicit QSServer(int &argc,char *argv[]);
    ~QSServer();
    typedef std::pair<QSRelevance,QSServer::routes_type::iterator> route_return_type;
    route_return_type select_route(QSRequest *request);
    void startRequestThread(qintptr socketDescriptor);
    bool eventFilter(QObject *obj, QEvent *event);

    void getStatus(json::value *status)const;

    operator const char*()const;
    friend std::ostream &operator<<(std::ostream &stream,const QSServer &rel);
    friend QDebug &operator<<(QDebug &stream,const QSServer &rhs);
public slots:
    void stop();
    void start();
    virtual void incomingConnection(qintptr socketDescriptor);
};

#endif // QSSERVER_H
