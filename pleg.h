#ifndef PLEG_H
#define PLEG_H

#include <iostream>
#include <cstdio>

#include <boost/uuid/string_generator.hpp>

#include <boost/asio.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/for_each.hpp>

typedef mpl::vector<bool, short, unsigned short, int, unsigned int, long, unsigned long, long long, unsigned long long, float, double, long double, std::string> value_types;
struct stream_operator_impl {
    stream_operator_impl(std::ostream &_strm, const boost::any& _p):strm(_strm),p(_p){}
    template <typename Any>
    std::ostream& operator()(Any &){
        if(p.type()==typeid(Any)){
            strm << any_cast<Any>(p);
        }
        return strm;
    }
private:
    std::ostream &strm;
    const boost::any& p;
};
std::ostream& operator<< (std::ostream& strm, const boost::any& p) {
    mpl::for_each<value_types>(stream_operator_impl(strm,p));
    return strm;
}

namespace Pleg
{
    class Server;

    extern boost::uuids::string_generator string_gen;

    extern int retval;
    extern const char *nullstr;

    extern Server *main_server;
    //FIXME
    //std::ostream &operator <<(std::ostream &stream, const byte_array &bytes);
}

#define Debug() std::cerr
#define Critical() std::cerr << "********"

#endif // PLEG_H
