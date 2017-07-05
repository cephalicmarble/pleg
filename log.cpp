#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include "log.h"

#include "server.h"

namespace Pleg {

Log::Log():lock(main_server->mutex)
{
}

Log::~Log()
{
    main_server->getLog().push_back(ss.str());
}

Log &Log::operator<<(std::string t)
{
    if(!empty){
        ss << " " << t;
    }else{
        ss << t;
        empty = false;
    }
    return *this;
}

} // namespace Pleg
