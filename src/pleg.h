#ifndef PLEG_H
#define PLEG_H

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/string_generator.hpp>
using namespace boost;
#include "plegapp.h"

namespace Pleg
{
    class Server;

    extern boost::uuids::nil_generator nil_gen;
    extern boost::uuids::string_generator string_gen;

    extern int retval;
    extern const char *nullstr;

    extern Server *main_server;
}

#endif // PLEG_H
