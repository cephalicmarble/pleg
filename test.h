#ifndef TEST_H
#define TEST_H

#include <list>
#include <map>
#include <memory>
using namespace std;
#include <boost/thread/recursive_mutex.hpp>
using namespace boost;
#include "object.h"
#include "thread.h"
#include "socket.h"
#include "signalhandler.h"
#include "application.h"
using namespace drumlin;
//#include "bluetooth.h"

namespace Pleg {

/**
 * @brief The TestType enum : The type of the test
 */
#define TestTypes (\
    GET,\
    POST,\
    PATCH,\
    UDP\
)
#define ENUM(TestType,TestTypes)

class Test;
typedef std::map<Test*,Socket*> socket_tests_type;

/**
 * @brief handler for the test sockets
 */
class SocketTestHandler :
        public SocketHandler
{
protected:
    tringList headers;
    qint64 contentLength = -1;
    tring content;
public:
    SocketTestHandler();
    virtual ~SocketTestHandler(){}
    virtual bool processTransmissionImpl(Socket*);
    virtual bool receivePacketImpl(Socket*);
    virtual bool readyProcessImpl(Socket*);
    virtual bool replyImpl(Socket*);
    virtual void completingImpl(Socket*,quint32);
    virtual void sortImpl(Socket*,Socket::buffers_type &);
    virtual void disconnectedImpl(Socket*);
};

/**
 * @brief The Test class : Runnable socket test class
 */
class Test :
    public ThreadWorker,
    public SocketTestHandler
{
    Q_OBJECT

    static QMutex mutex;
    TestType type;
    tring host;
    quint16 port;
    tring url;
    tringList headers;
public:
    Test(Thread *_thread,
           tring _host = "localhost",
           quint16 _port = 4999,
           TestType _type = GET);
    virtual ~Test();
    Test *setRelativeUrl(tring url);
    Test *setHeaders(const tringList &_headers);
    virtual void writeToStream(std::ostream &stream)const;
    virtual void shutdown();
    virtual void getStatus(json::value *status)const{}
public slots:
    virtual void run(QObject *obj,Event *event);
};

/**
 * @brief The TestLoop struct
 */
struct TestLoop :
    public Application<QCoreApplication>
{
    Q_OBJECT
    typedef Application<QCoreApplication> Base;
public:
    static QMutex mutex;
    TestLoop(int &argc,char *argv[]);
    Bluetooth *startBluetooth(const char *task,const tring &config);
    bool eventFilter(QObject *obj, QEvent *event);
};

#endif // TEST_H
