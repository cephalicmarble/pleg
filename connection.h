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

template <class Protocol = asio::ip::tcp>
class AsioClient
{
public:
    typedef typename Protocol::endpoint Endpoint;
    typedef typename Protocol::socket Socket;
    AsioClient(boost::asio::io_service& io_service,string host,int port)
        :m_endpoint(asio::ip::address::from_string(host),port),
         m_socket(io_service)
    {
        m_socket.connect(m_endpoint);
    }
    Endpoint m_endpoint;
    Socket m_socket;
    void disconnect()
    {
        m_socket.close();
    }
};

template <class Protocol = asio::ip::tcp>
class Connection
  : public boost::enable_shared_from_this<Connection<Protocol>>
{
public:
    typedef drumlin::Socket<Protocol> socket_type;

    socket_type & socket()
    {
        return m_socket;
    }

    virtual void connection_start()=0;
    virtual void error(boost::system::error_code ec)=0;

    Connection()
        : m_socket(io_service)
    {
    }
    ~Connection()
    {
        Critical() << "used to call socket.close()";
    }
private:
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
        }else{
            Critical() << error.message();
            delete new_connection;
        }
        start();
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
