#ifndef TEST_H
#define TEST_H

#include <list>
#include <map>
#include <memory>
using namespace std;
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>
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
template <class SockType>
class SocketTestHandler :
        public SocketHandler<SockType>
{
protected:
    vector<string> headers;
    gint64 contentLength = -1;
    string content;
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
template <class Protocol = asio::ip::tcp>
class Test :
    public ThreadWorker,
    public SocketTestHandler<typename Protocol::socket>
{
    static recursive_mutex mutex;
    TestType type;
    string host;
    guint16 port;
    string url;
    vector<string> headers;
public:
    /**
     * @brief Test::Test
     * @param parent Object
     * @param _host host name or ip
     * @param _port number
     * @param _type TestType
     */
    Test(Thread *_thread,
           string _host = "localhost",
           guint16 _port = 4999,
           TestType _type = GET)
        : ThreadWorker(_thread),SocketTestHandler()
    {
        tracepoint;
        type = _type;
        host = _host;
        port = _port;
    }
    /**
     * @brief Test::~Test
     */
    ~Test()
    {
        tracepoint;
    }
    Test *setRelativeUrl(string url);
    Test *setHeaders(vector<headers> &_headers);
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
    public Application<PlegApplication>
{
public:
    TestLoop(int &argc,char *argv[]);
};

} // namespace Pleg

#endif // TEST_H
