#include "exception.h"
#include <drumlin.h>

namespace drumlin {

Exception::Exception() : exception()
{
    message = "Unknown Exception";
}

Exception::Exception(const Exception &rhs) : exception()
{
    message = rhs.message;
}

Exception::Exception(const string &str) : exception()
{
    message = str;
}

} // namespace drumlin
