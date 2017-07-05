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

template <class Endpoint = asio::ip::tcp::endpoint,class Socket = asio::ip::tcp::socket>
class AsioClient
{
public:
    AsioClient(boost::asio::io_service& io_service,string host,int port)
    {
        m_endpoint = Endpoint(asio::ip::address::from_string(host),port);
        m_socket = Socket(io_service);
        m_socket.connect(m_endpoint);
    }
    Endpoint &m_endpoint;
    Socket &m_socket;
    void disconnect()
    {
        m_socket.close();
    }
};

template <class FullType,class SockType = asio::ip::tcp::socket>
class Connection
  : public boost::enable_shared_from_this<Connection>
{
public:
    typedef Socket<SockType> socket_type;

    static pointer create()
    {
        return pointer(new FullType());
    }

    socket_type& socket()
    {
        return m_socket;
    }

    virtual void connection_start()=0;

private:
    Connection()
        : m_socket(io_service)
    {
    }
    ~Connection()
    {
        m_socket.close();
    }
    socket_type m_socket;
};

template <
        class Connection = Connection<typename Protocol::socket>,
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
    typedef AsioServer<connection_type,address_type,endpoint_type,socket_type,acceptor_type> server_type;
    AsioServer(int port)
    {
        m_endpoint = Endpoint(Address.any(),port);
        m_acceptor = Acceptor(Pleg::io_service,m_endpoint);
    }
    void start()
    {
        m_acceptor.listen(10);
        typename connection_type::pointer new_connection = connection_type::create();
        m_acceptor.async_accept(new_connection->socket(),
            boost::bind(&server_type::handle_accept, this, new_connection, boost::asio::placeholders::error));
    }
    virtual void handle_accept(typename connection_type::pointer new_connection,const boost::system::error_code &error)
    {
        if(!error)
        {
            new_connection->connection_start();
            start();
        }
    }
    endpoint_type &m_endpoint;
    acceptor_type &m_acceptor;
    void stop()
    {
        m_acceptor.close();
    }
};

} // namespace drumlin

#endif // CONNECTION_H
