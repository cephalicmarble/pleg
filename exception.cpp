#include "exception.h"
#include <pleg.h>
using namespace Pleg;

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
