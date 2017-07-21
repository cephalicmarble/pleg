#ifndef TEST_H
#define TEST_H

#include <tao_forward.h>
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
#include "object.h"
#include "event.h"
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
protected:
    byte_array::size_type contentLength = 0;
    byte_array content;
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
public:
    typedef Protocol protocol_type;
    typedef SocketTestHandler<Protocol> SockHandler;
    typedef drumlin::Socket<Protocol> Socket;
    typedef AsioClient<Protocol> Client;
    /**
     * @brief Test::Test
     * @param parent Object
     * @param _host host name or ip
     * @param _port number
     * @param _type TestType
     */
    Test(string _host,
           string _port,
           TestType _type)
        : ThreadWorker(ThreadType_test,(Object*)0),SockHandler(),Client(drumlin::io_service,_host,lexical_cast<int>(_port))
        ,m_type(_type),host(_host),port(_port),m_socket(drumlin::io_service,(Object*)0,this,Client::getAsioSocket())
    {
        tracepoint;
        std::lock_guard<std::recursive_mutex> l(m_critical_section);
        m_thread = new Thread(metaEnum<TestType>().toString(_type));
        m_thread->setWorker(this);
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
    Test<Protocol> &setRelativeUrl(string _url)
    {
        tracepoint
        url = _url;
        string task;
        switch(m_type){
        case test_GET:task = "GET";break;
        case test_POST:task = "POST";break;
        case test_PATCH:task = "PATCH";break;
        case test_UDP:task = "UDP";break;
        }
        task += " " + url;
        getThread()->setTask(task);
        return *this;
    }
    /**
     * @brief Test::setHeaders : convenience function
     * @param _headers tring
     * @return Test*
     */
    Test<Protocol> &setHeaders(const string_list &_headers)
    {
        tracepoint
        headers = _headers;
        return *this;
    }

    /**
     * @brief Test::writeToStream : convenience for output
     * @param stream std::ostream
     */
    void writeToStream(std::ostream &stream)const
    {
        tracepoint
        string_list list;
        switch(m_type){
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
    void work(Object *,Event *)
    {
        if(!url.length())
            return;
        tracepoint
        m_socket
            .setTag((void*)getThread())
            .setHandler(this);

        Debug() << &m_socket << " opened" << boost::this_thread::get_id();
        string_list protocol;
        switch(m_type){
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
        m_socket.write(algorithm::join(protocol," ") + "\r\n" + headers.join("\r\n") + "\r\n\r\n");
        Debug() << &m_socket << " writing";
        m_socket.synchronousReads(true);
        m_socket.synchronousWrites(true);
        m_socket.writing();
        Debug() << &m_socket << " spinning";
        m_socket.reading();
        signalTermination();
    }

    /**
     * @brief Test<>::processTransmission handles HTTP
     * @param socket Socket*
     * @return bool
     */
    bool processTransmission(Socket *socket)
    {
        tracepoint
        byte_array bytes(socket->peekData(Socket::FlushBehaviours::CoalesceAndFlush));
        SockHandler::content.append(bytes.data(),bytes.length());
        return 0 < SockHandler::contentLength && SockHandler::content.length() >= SockHandler::contentLength;
    }

    /**
     * @brief Test<>::receivePacket handles UDP
     * @param socket Socket*
     * @return bool
     */
    bool receivePacket(Socket *socket)
    {
        tracepoint
        Debug() << socket->peekData(Socket::FlushBehaviours::CoalesceAndFlush).length();
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
        std::regex rx("^HTTP/1.1 [^\r\n]+\r\n([^\r\n]+\r\n)+\r\n(.*)");
        std::cmatch cap;
        byte_array bytes(socket->peekData(Socket::FlushBehaviours::Coalesce));
        cout << bytes.cdata();
        if(!std::regex_match(bytes.cdata(),cap,rx)){
            return false;
        }
        headers.clear();
        headers = string_list::fromString(cap[1],"\r\n",true);
        SockHandler::content = byte_array::fromRawData(bytes.cdata(),cap.length(0)-cap.length(2),string::npos);
        for(string const& header : headers){
            string_list nv(string_list::fromString(header,":"));
            if(nv[0] == "Content-Length"){
                SockHandler::contentLength = std::atoi(nv[1].c_str());
            }
        }
        if(0 < SockHandler::contentLength && SockHandler::content.length() >= SockHandler::contentLength) {
            SockHandler::content.truncate(SockHandler::contentLength);
            socket->peekData(Socket::FlushBehaviours::Flush);
            return true;
        }else{
            return false;
        }
    }
    /**
     * @brief Test<>::reply should write a reply to any communication
     * @param socket Socket*
     */
    bool reply(Socket *socket)
    {
        tracepoint
        socket->write(string("BLARGLE ARGLE"));
        socket->writing();
        return 0<=SockHandler::contentLength;
    }
    /**
     * @brief Test<>::completing writes a debug string
     * @param socket Socket*
     * @param written quint32
     */
    void completing(Socket *socket)
    {
        if(socket->writeQueueLength() || socket->socket().available())
            return;
        tracepoint
        if(!SockHandler::content.length() &&
           (!readyProcess(socket) || !processTransmission(socket))){
            Debug() << "early disconnection";
            Debug() << ((Thread*)socket->getTag()) << "quits";
            ((Thread*)socket->getTag())->quit();
            return;
        }
        Debug() << boost::this_thread::get_id() << __func__;
        std::cout << SockHandler::content.string() << endl;
        std::cout << SockHandler::contentLength << endl;
        try{
            json::to_stream(std::cout,json::from_string(string(SockHandler::content.cdata(),0,SockHandler::content.length())));
        }catch(std::exception &e){
            Critical() << e.what();
        }
        Debug() << ((Thread*)socket->getTag()) << "quits";
        ((Thread*)socket->getTag())->quit();
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
    void disconnected(Socket *)
    {
    }
    void error(Socket *socket,boost::system::error_code &ec)
    {
        Critical() << ec.message();
        ((Thread*)socket->getTag())->quit();
    }

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
    public Application<PlegApplication>
{
public:
    typedef Application<PlegApplication> Base;
    TestLoop(int &argc,char *argv[]);
};

} // namespace Pleg

#endif // TEST_H
