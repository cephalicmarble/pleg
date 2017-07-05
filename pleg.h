#ifndef PLEG_H
#define PLEG_H

#include <iostream>
#include <cstdio>
using namespace std;
#include <boost/uuid/string_generator.hpp>

#include <boost/asio.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/for_each.hpp>
using namespace boost;

typedef mpl::vector<bool, short, unsigned short, int, unsigned int, long, unsigned long, long long, unsigned long long, float, double, long double, std::string> value_types;
struct stream_operator_impl {
    stream_operator_impl(ostream &_strm, const boost::any& _p):strm(_strm),p(_p){}
    template <typename Any>
    ostream& operator()(Any &){
        if(p.type()==typeid(Any)){
            strm << any_cast<Any>(p);
        }
        return strm;
    }
private:
    ostream &strm;
    const boost::any& p;
};
extern ostream& operator<< (ostream& strm, const boost::any& p);
extern ostream& operator<< (ostream& strm, const vector<string> &vecS);
extern ostream& operator<< (ostream &strm, const byte_array &bytes);

namespace Pleg
{
    class Server;

    extern boost::uuids::string_generator string_gen;

    extern int retval;
    extern const char *nullstr;

    extern Server *main_server;
    //FIXME
}

#define Debug() std::cerr
#define Critical() std::cerr << "********"

#endif // PLEG_H
