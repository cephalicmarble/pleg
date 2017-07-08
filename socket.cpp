#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include <algorithm>
using namespace std;
#include <boost/thread/recursive_mutex.hpp>
using namespace boost;
#include "socket.h"
#include "exception.h"
#include "thread.h"
#include "connection.h"

namespace drumlin {

template<>
SocketAdapter<asio::ip::tcp>::SocketAdapter(Socket<asio::ip::tcp> *socket):m_socket(socket)
{
}

template<>
void SocketAdapter<asio::ip::tcp>::error(boost::system::error_code ec)
{
    m_socket->m_connection->error(ec);
}

template <>
void Socket<asio::ip::tcp>::getStatus(json::value &status)
{
    json::object_t &obj(status.get_object());
    obj.insert({"available",socket().available()});
    obj.insert({"buffered",bytesToWrite()});
}

template<>
SocketAdapter<asio::ip::udp>::SocketAdapter(Socket<asio::ip::udp> *socket):m_socket(socket)
{
}

template<>
void SocketAdapter<asio::ip::udp>::error(boost::system::error_code ec)
{
    m_socket->m_connection->error(ec);
}

template <>
void Socket<asio::ip::udp>::getStatus(json::value &status)
{
    json::object_t &obj(status.get_object());
    obj.insert({"available",socket().available()});
    obj.insert({"buffered",bytesToWrite()});
}

} // namespace drumlin
