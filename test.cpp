#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include <algorithm>
using namespace std;
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/algorithm/string.hpp>
using namespace boost;
#include "plegapp.h"
#include "thread.h"
#include "test.h"
#include "event.h"
#include "thread.h"
#include "cursor.h"
#include "application.h"
#include "glib.h"
using namespace drumlin;

namespace Pleg {

recursive_mutex Test::mutex;

/**
 * @brief Test::run defines the test HTTP phrases, UDP preamble
 */
void Test::run(QObject *obj,Event *event)
{
    tracepoint
    lock_guard l(&mutex);
    Socket::SocketType socketType =
        type==TestType::UDP?Socket::SocketType::UdpSocket
                           :Socket::SocketType::TcpSocket;
    Socket socket(socketType,0,this);
    socket.setTag((void*)getThread());
    socket.connectToHost(host,port);
    socket.open(Socket::ReadWrite);
    qDebug() << &socket << " opened";
    ml.unlock();
    socket.waitForConnected(1000);
    tringList protocol;
    switch(type){
    case UDP:
        protocol << "HELO";
        break;
    case GET:
        protocol << "GET" << url << "HTTP/1.1";
        break;
    case POST:
        protocol << "POST" << url << "HTTP/1.1";
        break;
    case PATCH:
        protocol << "GET" << url << "HTTP/1.1";
        break;
    }
    socket.write((protocol.join(" ").toStdString() + "\r\n" + headers.join("\r\n").toStdString() + "\r\n\r\n").c_str());
    qDebug() << &socket << " writing";
    socket.flushWriteBuffer();
    qDebug() << &socket << " spinning";
    socket.waitForReadyRead(1000);
}

/**
 * @brief Test::setRelativeUrl : convenience function
 * @param _url tring
 * @return Test*
 */
Test *Test::setRelativeUrl(tring _url)
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
Test *Test::setHeaders(const tringList &_headers)
{
    tracepoint
    headers = _headers;
    return this;
}

/**
 * @brief Test::writeToStream : convenience for output
 * @param stream std::ostream
 */
void Test::writeToStream(std::ostream &stream)const
{
    tracepoint
    tringList list;
    switch(type){
    case UDP:
        list << "UDP";
        break;
    case GET:
        list << "GET" << url << "HTTP/1.1";
        break;
    case POST:
        list << "POST" << url << "HTTP/1.1";
        break;
    case PATCH:
        list << "PATCH" << url << "HTTP/1.1";
        break;
    }
    stream << list.join(" ").toStdString() << endl;
}

/**
 * @brief Test::shutdown : probably
 */
void Test::shutdown()
{
    signalTermination();
}

/**
 * @brief SocketTestHandler::SocketTestHandler is empty
 */
SocketTestHandler::SocketTestHandler() : SocketHandler()
{
    tracepoint
}

/**
 * @brief SocketTestHandler::processTransmissionImpl handles HTTP
 * @param socket Socket*
 * @return bool
 */
bool SocketTestHandler::processTransmissionImpl(Socket *socket)
{
    tracepoint
    content += tring(socket->peekData(Socket::FlushBehaviours::CoalesceAndFlush));
    return 0 <= contentLength && content.length() >= contentLength;
}

/**
 * @brief SocketTestHandler::receivePacketImpl handles UDP
 * @param socket Socket*
 * @return bool
 */
bool SocketTestHandler::receivePacketImpl(Socket *socket)
{
    tracepoint
    QByteArray bytes(socket->peekData(Socket::FlushBehaviours::CoalesceAndFlush));
    qDebug() << bytes.length();
    return true;
}

/**
 * @brief SocketTestHandler::readyProcessImpl is always ready to reply
 * @param socket Socket*
 * @return bool
 */
bool SocketTestHandler::readyProcessImpl(Socket *socket)
{
    tracepoint
    qDebug() << __func__;
    QByteArray bytes(socket->peekData(Socket::FlushBehaviours::Coalesce));
    QRegExp rx("^HTTP/1.1 [^\r\n]+\r\n([^\r\n]+\r\n)+\r\n(.*)");
    tring http(bytes);
    if(!rx.exactMatch(http)){
        return false;
    }
    socket->peekData(Socket::FlushBehaviours::Flush);
    content = tring(rx.cap(2).toStdString().c_str());
    headers = rx.cap(1).split("\r\n",tring::SplitBehavior::SkipEmptyParts);
    for(tring const& header : headers){
        QVector<tringRef> nv(header.splitRef(":",tring::SplitBehavior::SkipEmptyParts));
        if(nv[0].toString() == "Content-Length"){
            contentLength = nv[1].toInt();
        }
    }
    return 0<=contentLength;
}

/**
 * @brief SocketTestHandler::replyImpl should write a reply to any communication
 * @param socket Socket*
 */
bool SocketTestHandler::replyImpl(Socket *socket)
{
    tracepoint
    socket->write("BLARGLE ARGLE");
    qDebug() << socket << " wrote " << socket->flushWriteBuffer() << " bytes.";
    return 0<=contentLength;
}

/**
 * @brief SocketTestHandler::completingImpl writes a debug string
 * @param socket Socket*
 * @param written quint32
 */
void SocketTestHandler::completingImpl(Socket *socket,quint32 written)
{
    tracepoint
    qDebug() << socket << " wrote " << written << " bytes.";
    socket->close();
}

/**
 * @brief SocketTestHandler::sortImpl reverses the input buffers' ordering
 * @param socket Socket*
 * @param buffers QVector<QByteArray>
 */
void SocketTestHandler::sortImpl(Socket *socket,Socket::buffers_type &buffers)
{
    tracepoint
    if(socket->socketType()!=Socket::SocketType::UdpSocket)
        return;
    reverse(buffers.begin(),buffers.end());
}

/**
 * @brief SocketTestHandler::disconnectedImpl interrupt the test thread
 * @param socket Socket*
 */
void SocketTestHandler::disconnectedImpl(Socket *socket)
{
    tracepoint
    if(!content.length() &&
       (!readyProcessImpl(socket) || !processTransmissionImpl(socket))){
        qDebug() << "early disconnection";
        qDebug() << ((Thread*)socket->getTag()) << "quits";
        ((Thread*)socket->getTag())->quit();
        return;
    }
    qDebug() << QThread::currentThread() << __func__;
    QJsonDocument doc;
    QJsonParseError error;
    if(!doc.fromJson(QByteArray(content.trimmed().toStdString().c_str(),content.length()),&error).isNull()){
        json::to_stream(std::cout,json::from_string(content.toStdString()),2);
    }else{
        qDebug() << error.errorString();
        qDebug() << content;
    }
    qDebug() << ((Thread*)socket->getTag()) << "quits";
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
