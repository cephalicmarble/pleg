#ifndef LOG_H
#define LOG_H

#include <pleg.h>
#include <sstream>

namespace Pleg {

class Log
{
    std::stringstream ss;
    bool empty = true;
public:
    Log();
    ~Log();
    Log &operator<<(std::string t);
};

} // namespace Pleg

extern Pleg::Log &log();
#define Log() log()

#endif // LOG_H
