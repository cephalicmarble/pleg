#ifndef LOG_H
#define LOG_H

#include <pleg.h>
#include <sstream>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>
using namespace boost;

namespace Pleg {

class Log
{
    std::stringstream ss;
    bool empty = true;
    lock_guard<recursive_mutex> lock;
public:
    Log();
    ~Log();
    Log &operator<<(std::string t);
};

} // namespace Pleg

extern Pleg::Log log();

#endif // LOG_H
