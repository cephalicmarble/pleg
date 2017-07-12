#ifndef CONNECTION_H
#define CONNECTION_H

#include <memory>
using namespace std;
#include <boost/asio.hpp>
using namespace boost;
#include "glib.h"
#include "socket.h"

namespace drumlin {

extern boost::asio::io_service io_service;

class IOService
{
public:
    IOService():m_thread(&IOService::run,this){}
    void run(){io_service.run();}
    void stop(){io_service.stop();}
private:
    thread m_thread;
};

extern unique_ptr<IOService> io_thread;
extern void start_io();

template <class Protocol = asio::ip::tcp>
class AsioClient
{
public:
    typedef Protocol protocol_type;
    typedef typename protocol_type::resolver resolver_type;
    typedef typename Protocol::endpoint Endpoint;
    typedef typename Protocol::socket Socket;
    AsioClient(boost::asio::io_service& io_service,typename resolver_type::iterator resolver_iter)
        :m_endpoint(*resolver_iter),m_socket(io_service)
    {
        m_socket.connect(m_endpoint);
    }
    AsioClient(boost::asio::io_service& io_service,string host,int port)
        :m_endpoint(asio::ip::address::from_string(host),port),
         m_socket(io_service)
    {
        m_socket.connect(m_endpoint);
    }
    void disconnect()
    {
        m_socket.close();
    }
    Socket *getAsioSocket()
    {
        return &m_socket;
    }
protected:
    Endpoint m_endpoint;
    Socket m_socket;
//    static AsioClient<Protocol> resolve(boost::asio::io_service,string host,int port)
//    {
//        resolver_type resolver(io_service);
//        typename protocol_type::resolver::query query(host,port);
//        typename resolver_type::iterator iter = resolver.resolve(query);
//        typename resolver_type::iterator end;
//        while(iter != end){
//            typename protocol_type::endpoint endpoint = *iter++;
//            std::cout << endpoint << std::endl;
//        }
//        return true;
//    }
};

template <class Protocol = asio::ip::tcp>
class Connection
  : public boost::enable_shared_from_this<Connection<Protocol>>
{
public:
    typedef typename Protocol::socket asio_socket_type;
    typedef drumlin::Socket<Protocol> socket_type;
    typedef SocketHandler<Protocol> handler_type;

    socket_type & socket()
    {
        return m_socket;
    }

    virtual void connection_start()=0;
    virtual void error(boost::system::error_code ec)=0;

    Connection(handler_type *handler)
        : m_asio_socket(io_service),m_socket(io_service,0,handler,&m_asio_socket)
    {
    }
    ~Connection()
    {
    }
private:
    asio_socket_type m_asio_socket;
    socket_type m_socket;
};

template <
        class Connection,
        class Address = asio::ip::address_v4,
        class Protocol = asio::ip::tcp>
class AsioServer
{
public:
    typedef Connection connection_type;
    typedef Address address_type;
    typedef typename Protocol::endpoint endpoint_type;
    typedef typename Protocol::socket socket_type;
    typedef typename Protocol::acceptor acceptor_type;
    typedef typename Protocol::resolver resolver_type;
    typedef AsioServer<connection_type,address_type,Protocol> server_type;
    AsioServer(string host,int port):addr(),m_endpoint(host.length()?addr.from_string(host):addr.any(),port),m_acceptor(acceptor_type(drumlin::io_service,m_endpoint))
    {
    }
    void start()
    {
        m_acceptor.listen(10);
        connection_type *new_connection = new connection_type(dynamic_cast<Pleg::Server*>(this));
//        m_acceptor.accept(new_connection->socket().socket());
//        cout << new_connection->socket().socket().available() << endl;
//        new_connection->socket().socket().send(asio::buffer(string("BLARGLE")));
        m_acceptor.async_accept(new_connection->socket().socket(),
            boost::bind(&server_type::handle_accept, this, new_connection, boost::asio::placeholders::error));
    }
    virtual void handle_accept(connection_type *new_connection,boost::system::error_code error)
    {
        if(!error)
        {
            new_connection->connection_start();
            start();
        }else{
            Critical() << error.message();
            delete new_connection;
        }
    }
    address_type addr;
    endpoint_type m_endpoint;
    acceptor_type m_acceptor;
    void stop()
    {
        m_acceptor.close();
    }
};

} // namespace drumlin

#endif // CONNECTION_H
