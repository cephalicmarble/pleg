#include "exception.h"
#include <pleg.h>
using namespace Pleg;

namespace drumlin {

Exception::Exception() : std::exception()
{
    message = "Unknown Exception";
}

Exception::Exception(const Exception &rhs) : std::exception()
{
    message = rhs.message;
}

Exception::Exception(const string &str) : std::exception()
{
    message = str;
}

} // namespace drumlin
