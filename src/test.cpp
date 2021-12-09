#define TAOJSON
#include "test.h"

#include <algorithm>
#include <mutex>
using namespace std;
#include <boost/algorithm/string.hpp>
using namespace boost;
#include "drumlin/thread.h"
#include "drumlin/event.h"
#include "drumlin/thread.h"
#include "drumlin/cursor.h"
#include "drumlin/application.h"
#include "drumlin/connection.h"
using namespace drumlin;
#include "pleg.h"
using namespace Pleg;

namespace Pleg {

Test::Test(string _host,
         string _port,
         TestType _type)
    : ThreadWorker(ThreadType_test,(Object*)0),
    SockHandler(),
    Client(drumlin::io_service,_host,lexical_cast<int>(_port)),
    m_type(_type),
    host(_host),
    port(_port),
    m_socket(drumlin::io_service,(Object*)0,this,Client::getAsioSocket())
{
    tracepoint;
    std::lock_guard<std::recursive_mutex> l(m_critical_section);
    m_thread = new Thread(metaEnum<TestType>().toString(_type));
    m_thread->setWorker(this);
}
/**
 * @brief Test::~Test
 */
Test::~Test()
{
    tracepoint;
}
/**
 * @brief Test::setRelativeUrl : convenience function
 * @param _url tring
 * @return Test*
 */
Test &Test::setRelativeUrl(string _url)
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
Test &Test::setHeaders(const string_list &_headers)
{
    tracepoint
    headers = _headers;
    return *this;
}

/**
 * @brief Test::writeToStream : convenience for output
 * @param stream std::ostream
 */
void Test::writeToStream(std::ostream &stream)const
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
void Test::shutdown()
{
    signalTermination();
}

/**
 * @brief Test::run defines the test HTTP phrases, UDP preamble
 */
void Test::work(Object *,Event *)
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
 * @brief Test::processTransmission handles HTTP
 * @param socket Socket*
 * @return bool
 */
bool Test::processTransmission(Socket *socket)
{
    tracepoint
    byte_array bytes(socket->peekData(Socket::FlushBehaviours::CoalesceAndFlush));
    SockHandler::content.append(bytes.data(),bytes.length());
    return 0 < SockHandler::contentLength && SockHandler::content.length() >= SockHandler::contentLength;
}

/**
 * @brief Test::receivePacket handles UDP
 * @param socket Socket*
 * @return bool
 */
bool Test::receivePacket(Socket *socket)
{
    tracepoint
    Debug() << socket->peekData(Socket::FlushBehaviours::CoalesceAndFlush).length();
    return true;
}

/**
 * @brief Test::readyProcess is always ready to reply
 * @param socket Socket*
 * @return bool
 */
bool Test::readyProcess(Socket *socket)
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
 * @brief Test::reply should write a reply to any communication
 * @param socket Socket*
 */
bool Test::reply(Socket *socket)
{
    tracepoint
    socket->write(string("BLARGLE ARGLE"));
    socket->writing();
    return 0<=SockHandler::contentLength;
}

/**
 * @brief Test::completing writes a debug string
 * @param socket Socket*
 * @param written quint32
 */
void Test::completing(Socket *socket)
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
 * @brief Test::sort reverses the input buffers' ordering
 * @param socket Socket*
 * @param buffers QVector<QByteArray>
 */
void Test::sort(Socket *,drumlin::buffers_type &buffers)
{
    tracepoint
    reverse(buffers.begin(),buffers.end());
}

/**
 * @brief Test::disconnected interrupt the test thread
 * @param socket Socket*
 */
void Test::disconnected(Socket *)
{
}

void Test::error(Socket *socket,boost::system::error_code &ec)
{
    Critical() << ec.message();
    ((Thread*)socket->getTag())->quit();
}


///**
// * @brief TestLoop::startBluetooth : thread starter
// * @param task const char*
// * @param config tring const&
// * @return Bluetooth* (BluetoothScanner*) if !strcmp(task,"scan")
// */
//Bluetooth *TestLoop::startBluetooth(const char *task,const tring &config)
//{
//    tracepoint
//    QMutexLocker ml(&mutex);
//    Thread *thread(new Thread(task));
//    Bluetooth *bluet;
//    if(!strcmp(task,"scan")){
//        bluet = new BluetoothScanner(thread,config);
//    }else{
//        bluet = new Bluetooth(thread);
//    }
//    app->addThread(thread,true);
//    return bluet;
//}

TestLoop::TestLoop(int &argc,char *argv[])
    : Base(argc,argv)
{
    terminator = (Terminator*)1;
}

template class SocketTestHandler<asio::ip::tcp>;
template class SocketTestHandler<asio::ip::udp>;

} // namespace Pleg
