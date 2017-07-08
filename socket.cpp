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
drumlin::buffers_type &SocketAdapter<asio::ip::tcp>::getWriteBuffers()
{
    return m_socket->writeBuffers;
}

template<>
drumlin::buffers_type &SocketAdapter<asio::ip::tcp>::getReadBuffers()
{
    return m_socket->readBuffers;
}

template<>
bool SocketAdapter<asio::ip::tcp>::available()
{
    return m_socket->socket().available();
}

template<>
void SocketAdapter<asio::ip::tcp>::process()
{
    m_socket->process();
}

template<>
void SocketAdapter<asio::ip::tcp>::async_receive(ISocketTask *task)
{
    socket_type::readHandler_type *handler(dynamic_cast<socket_type::readHandler_type*>(task));
    m_socket->socket().async_receive(asio::buffer(handler->recv_buf.data(),handler->recv_buf.max_size()),*handler);
}

template<>
void SocketAdapter<asio::ip::tcp>::async_send(ISocketTask *task)
{
    socket_type::writeHandler_type *handler(dynamic_cast<socket_type::writeHandler_type*>(task));
    m_socket->socket().async_send(asio::buffer(handler->p_buffer->data<void>(),handler->p_buffer->length()),*handler);
}

template<>
void SocketAdapter<asio::ip::tcp>::error(boost::system::error_code ec)
{
    m_socket->m_connection->error(ec);
}

template<>
void SocketAdapter<asio::ip::tcp>::completing(socket_type::writeHandler_type *task)
{
    m_socket->handler->completing(m_socket,task);
}

template<>
size_t SocketAdapter<asio::ip::tcp>::writeQueueLength()
{
    return m_socket->writeQueueLength();
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
drumlin::buffers_type &SocketAdapter<asio::ip::udp>::getWriteBuffers()
{
    return m_socket->writeBuffers;
}

template<>
drumlin::buffers_type &SocketAdapter<asio::ip::udp>::getReadBuffers()
{
    return m_socket->readBuffers;
}

template<>
bool SocketAdapter<asio::ip::udp>::available()
{
    return m_socket->socket().available();
}

template<>
void SocketAdapter<asio::ip::udp>::process()
{
    m_socket->process();
}

template<>
void SocketAdapter<asio::ip::udp>::async_receive(ISocketTask *task)
{
    socket_type::readHandler_type *handler(dynamic_cast<socket_type::readHandler_type*>(task));
    m_socket->socket().async_receive(asio::buffer(handler->recv_buf.data(),handler->recv_buf.max_size()),*handler);
}

template<>
void SocketAdapter<asio::ip::udp>::async_send(ISocketTask *task)
{
    socket_type::writeHandler_type *handler(dynamic_cast<socket_type::writeHandler_type*>(task));
    m_socket->socket().async_send(asio::buffer(handler->p_buffer->data<void>(),handler->p_buffer->length()),*handler);
}

template<>
void SocketAdapter<asio::ip::udp>::error(boost::system::error_code ec)
{
    m_socket->m_connection->error(ec);
}

template<>
void SocketAdapter<asio::ip::udp>::completing(socket_type::writeHandler_type *task)
{
    m_socket->handler->completing(m_socket,task);
}

template<>
size_t SocketAdapter<asio::ip::udp>::writeQueueLength()
{
    return m_socket->writeQueueLength();
}

template <>
void Socket<asio::ip::udp>::getStatus(json::value &status)
{
    json::object_t &obj(status.get_object());
    obj.insert({"available",socket().available()});
    obj.insert({"buffered",bytesToWrite()});
}

} // namespace drumlin
