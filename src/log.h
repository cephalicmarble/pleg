#ifndef LOG_H
#define LOG_H

#include "pleg.h"
#include <sstream>
#include <mutex>
using namespace std;

namespace Pleg {

class Log
{
    std::stringstream ss;
    bool empty = true;
    std::lock_guard<std::recursive_mutex> lock;
public:
    Log();
    ~Log();
    Log &operator<<(std::string t);
};

} // namespace Pleg

extern Pleg::Log log();

#endif // LOG_H
