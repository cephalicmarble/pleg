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

namespace drumlin {

template <>
void Socket<asio::ip::tcp>::getStatus(json::value &status)
{
    json::object_t &obj(status.get_object());
    obj.insert({"available",socket().available()});
    obj.insert({"buffered",bytesToWrite()});
}

template <>
void Socket<asio::ip::udp>::getStatus(json::value &status)
{
    json::object_t &obj(status.get_object());
    obj.insert({"available",socket().available()});
    obj.insert({"buffered",bytesToWrite()});
}

} // namespace drumlin
