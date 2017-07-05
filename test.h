#ifndef TEST_H
#define TEST_H

#include <list>
#include <map>
#include <memory>
using namespace std;
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
using namespace boost;
#include "object.h"
#include "cursor.h"
#include "thread.h"
#include "socket.h"
#include "signalhandler.h"
#include "application.h"
#include "connection.h"
using namespace drumlin;
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
    virtual bool processTransmission(Socket*)=0;
    virtual bool receivePacket(Socket*)=0;
    virtual bool readyProcess(Socket*)=0;
    virtual bool reply(Socket*)=0;
    virtual void completing(Socket*,gint64)=0;
    virtual void sort(Socket*,drumlin::buffers_type &)=0;
    virtual void disconnected(Socket*)=0;
protected:
    vector<string> headers;
    string::size_type contentLength = -1;
    string content = "";
};

/**
 * @brief The Test class : Runnable socket test class
 */
template <class Protocol = asio::ip::tcp>
class Test :
    public ThreadWorker,
    public SocketTestHandler<Protocol>,
    public AsioClient<Protocol>
{
    TestType type;
    string host;
    string port;
    string url;
    vector<string> headers;
    typedef Protocol protocol_type;
    typedef SocketTestHandler<Protocol> SockHandler;
    typedef drumlin::Socket<Protocol> Socket;
    typedef AsioClient<Protocol> Client;
public:
    /**
     * @brief Test::Test
     * @param parent Object
     * @param _host host name or ip
     * @param _port number
     * @param _type TestType
     */
    Test(Thread *_thread,
           string _host,
           string _port,
           TestType _type)
        : ThreadWorker(ThreadType_test,_thread),SockHandler(),Client(drumlin::io_service,host,std::atoi(port.c_str())),type(_type)
    {
        tracepoint;
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
    /**
     * @brief Test::setRelativeUrl : convenience function
     * @param _url tring
     * @return Test*
     */
    Test<Protocol> *setRelativeUrl(string _url)
    {
        tracepoint
        url = _url;
        return this;
    }
    /**
     * @brief Test::setHeaders : convenience function
     * @param _headers tring
     * @return Test*
     */
    Test<Protocol> *setHeaders(const string_list &_headers)
    {
        tracepoint
        headers = _headers;
        return this;
    }

    /**
     * @brief Test::writeToStream : convenience for output
     * @param stream std::ostream
     */
    void writeToStream(std::ostream &stream)const
    {
        tracepoint
        string_list list;
        switch(type){
        case test_UDP:
            list << "UDP";
            break;
        case test_GET:
            list << "GET" << url << "HTTP/1.1";
            break;
        case test_POST:
            list << "POST" << url << "HTTP/1.1";
            break;
        case test_PATCH:
            list << "PATCH" << url << "HTTP/1.1";
            break;
        }
        stream << list.join(" ") << endl;
    }

    /**
     * @brief Test::shutdown : probably
     */
    void shutdown()
    {
        signalTermination();
    }

    /**
     * @brief Test::run defines the test HTTP phrases, UDP preamble
     */
    void run(Object *obj,Event *)
    {
        tracepoint
                Socket socket(drumlin::io_service,obj,this);
        socket.setTag((void*)getThread());
        socket.connectToHost(host,port);
        Debug() << &socket << " opened";
        string_list protocol;
        switch(type){
        case test_UDP:
            protocol << "HELO";
            break;
        case test_GET:
            protocol << "GET" << url << "HTTP/1.1";
            break;
        case test_POST:
            protocol << "POST" << url << "HTTP/1.1";
            break;
        case test_PATCH:
            protocol << "GET" << url << "HTTP/1.1";
            break;
        }
        socket.write(algorithm::join(protocol," ") + "\r\n" + algorithm::join(headers,"\r\n") + "\r\n\r\n");
        Debug() << &socket << " writing";
        socket.flushWriteBuffer();
        Debug() << &socket << " spinning";
        while(socket.socket().available()){
            socket.ReadyRead();
        }
    }
    /**
     * @brief Test<>::processTransmission handles HTTP
     * @param socket Socket*
     * @return bool
     */
    bool processTransmission(Socket *socket)
    {
        tracepoint
        SockHandler::content += string(socket->peekData(Socket::FlushBehaviours::CoalesceAndFlush));
        return 0 <= SockHandler::contentLength && SockHandler::content.length() >= SockHandler::contentLength;
    }

    /**
     * @brief Test<>::receivePacket handles UDP
     * @param socket Socket*
     * @return bool
     */
    bool receivePacket(Socket *socket)
    {
        tracepoint
        byte_array bytes(socket->peekData(Socket::FlushBehaviours::CoalesceAndFlush));
        Debug() << bytes.length();
        return true;
    }
    /**
     * @brief Test<>::readyProcess is always ready to reply
     * @param socket Socket*
     * @return bool
     */
    bool readyProcess(Socket *socket)
    {
        tracepoint
        Debug() << __func__;
        byte_array bytes(socket->peekData(Socket::FlushBehaviours::Coalesce));
        boost::regex rx("^HTTP/1.1 [^\r\n]+\r\n([^\r\n]+\r\n)+\r\n(.*)");
        boost::smatch cap;
        string http(bytes.string());
        if(!boost::regex_match(http,cap,rx)){
            return false;
        }
        socket->peekData(Socket::FlushBehaviours::Flush);
        SockHandler::content = string(cap[2]);
        headers.clear();
        headers = string_list::fromString(SockHandler::content,"\r\n",true);
        for(string const& header : headers){
            string_list nv(string_list::fromString(header,":"));
            if(nv[0] == "Content-Length"){
                SockHandler::contentLength = std::atoi(nv[1].c_str());
            }
        }
        return 0<=SockHandler::contentLength;
    }
    /**
     * @brief Test<>::reply should write a reply to any communication
     * @param socket Socket*
     */
    bool reply(Socket *socket)
    {
        tracepoint
        socket->write(string("BLARGLE ARGLE"));
        Debug() << socket << " wrote " << socket->flushWriteBuffer() << " bytes.";
        return 0<=SockHandler::contentLength;
    }
    /**
     * @brief Test<>::completing writes a debug string
     * @param socket Socket*
     * @param written quint32
     */
    void completing(Socket *socket,gint64 written)
    {
        tracepoint
        Debug() << socket << " wrote " << written << " bytes.";
        socket->socket().close();
    }

    /**
     * @brief Test<>::sort reverses the input buffers' ordering
     * @param socket Socket*
     * @param buffers QVector<QByteArray>
     */
    void sort(Socket *,drumlin::buffers_type &buffers)
    {
        tracepoint
        reverse(buffers.begin(),buffers.end());
    }
    /**
     * @brief Test<>::disconnected interrupt the test thread
     * @param socket Socket*
     */
    void disconnected(Socket *socket)
    {
        tracepoint
        if(!SockHandler::content.length() &&
           (!readyProcess(socket) || !processTransmission(socket))){
            Debug() << "early disconnection";
            Debug() << ((Thread*)socket->getTag()) << "quits";
            ((Thread*)socket->getTag())->quit();
            return;
        }
        Debug() << this_thread::get_id() << __func__;
        algorithm::trim(SockHandler::content);
        json::to_stream(std::cout,json::from_string(SockHandler::content));
        Debug() << ((Thread*)socket->getTag()) << "quits";
        ((Thread*)socket->getTag())->quit();
    }
};

/**
 * @brief The TestLoop struct
 */
struct TestLoop :
    public Application<PlegApplication>
{
public:
    typedef Application<PlegApplication> Base;
    TestLoop(int &argc,char *argv[]);
};

} // namespace Pleg

#endif // TEST_H
