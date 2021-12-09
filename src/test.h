#ifndef TEST_H
#define TEST_H

#include "drumlin/tao_forward.h"
using namespace tao;
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <mutex>
using namespace std;
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
using namespace boost;
#include "drumlin/object.h"
#include "drumlin/event.h"
#include "drumlin/cursor.h"
#include "drumlin/thread.h"
#include "drumlin/thread_worker.h"
#include "drumlin/socket.h"
#include "drumlin/signalhandler.h"
#include "drumlin/application.h"
#include "drumlin/connection.h"
#include "drumlin/string_list.h"
using namespace drumlin;
#include "plegapp.h"
//#include "bluetooth.h"

/**
 * @brief The TestType enum : The type of the test
 */
#define TestTypes (\
    test_GET,\
    test_POST,\
    test_PATCH,\
    test_UDP\
)
ENUM(TestType,TestTypes)

namespace Pleg {

/**
 * @brief handler for the test sockets
 */
template <class Protocol>
class SocketTestHandler :
        public SocketHandler<Protocol>
{
public:
    typedef drumlin::Socket<Protocol> Socket;
protected:
    byte_array::size_type contentLength = 0;
    byte_array content;
};

/**
 * @brief The Test class : Runnable socket test class
 */
class Test :
    public ThreadWorker,
    public SocketTestHandler<asio::ip::tcp>,
    public AsioClient<asio::ip::tcp>
{
public:
    typedef asio::ip::tcp Protocol;
    typedef Protocol protocol_type;
    typedef SocketTestHandler<Protocol> SockHandler;
    typedef SockHandler::Socket Socket;
    typedef AsioClient<Protocol> Client;
    /**
     * @brief Test::Test
     * @param parent Object
     * @param _host host name or ip
     * @param _port number
     * @param _type TestType
     */
    Test(string _host, string _port, TestType _type);
    /**
     * @brief Test::~Test
     */
    ~Test();
    /**
     * @brief Test::setRelativeUrl : convenience function
     * @param _url tring
     * @return Test*
     */
    Test &setRelativeUrl(string _url);
    /**
     * @brief Test::setHeaders : convenience function
     * @param _headers tring
     * @return Test*
     */
    Test &setHeaders(const string_list &_headers);
    /**
     * @brief Test::writeToStream : convenience for output
     * @param stream std::ostream
     */
    void writeToStream(std::ostream &stream)const;
    /**
     * @brief Test::shutdown : probably
     */
    void shutdown();
    /**
     * @brief Test::run defines the test HTTP phrases, UDP preamble
     */
    void work(Object *,Event *);
    /**
     * @brief Test<>::processTransmission handles HTTP
     * @param socket Socket*
     * @return bool
     */
    bool processTransmission(Socket *socket);
    /**
     * @brief Test<>::receivePacket handles UDP
     * @param socket Socket*
     * @return bool
     */
    bool receivePacket(Socket *socket);
    /**
     * @brief Test<>::readyProcess is always ready to reply
     * @param socket Socket*
     * @return bool
     */
    bool readyProcess(Socket *socket);
    /**
     * @brief Test<>::reply should write a reply to any communication
     * @param socket Socket*
     */
    bool reply(Socket *socket);
    /**
     * @brief Test<>::completing writes a debug string
     * @param socket Socket*
     * @param written quint32
     */
    void completing(Socket *socket);
    /**
     * @brief Test<>::sort reverses the input buffers' ordering
     * @param socket Socket*
     * @param buffers QVector<QByteArray>
     */
    void sort(Socket *,drumlin::buffers_type &buffers);
    /**
     * @brief Test<>::disconnected interrupt the test thread
     * @param socket Socket*
     */
    void disconnected(Socket *);

    void error(Socket *socket,boost::system::error_code &ec);

private:
    TestType m_type;
    string host;
    string port;
    string url;
    string_list headers;
    Socket m_socket;
};

/**
 * @brief The TestLoop struct
 */
struct TestLoop :
    public PlegApplication
{
public:
    typedef PlegApplication Base;
    TestLoop(int &argc,char *argv[]);
};

} // namespace Pleg

#endif // TEST_H
