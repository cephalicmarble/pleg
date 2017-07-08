#ifndef PLEG_H
#define PLEG_H

#include <iostream>
#include <cstdio>
using namespace std;
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/algorithm/string.hpp>
using namespace boost;
#include "logger.h"
using namespace drumlin;

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
extern logger& operator<< (logger& strm, const boost::any& p);

class string_list : public vector<string>
{
public:
    typedef vector<string> base;
    friend string_list& operator<< (string_list &vecS,string const& str);
    friend string_list& operator<< (string_list &vecS,string & str);
    friend string_list& operator<< (string_list &vecS,const char* str);
    friend string_list& operator<< (string_list &vecS,char *str);
    string join(string str);
    string join(const char*pc);
    static string_list fromString(string const& toSplit,string delim,bool all = false,algorithm::token_compress_mode_type = algorithm::token_compress_on);
    static string_list fromString(string const& toSplit,const char* delim,bool all = false,algorithm::token_compress_mode_type = algorithm::token_compress_on);
    string_list &operator=(string_list &rhs);
    string_list():base(){}
    template<class transform_iter_type>
    string_list(transform_iter_type &begin,transform_iter_type &end)
    {
        std::copy(begin,end,back_inserter(*this));
    }
};

extern string_list & operator<< (string_list&,string const& str);
extern string_list & operator<< (string_list&,const char* str);
extern string_list & operator<< (string_list&,string & str);
extern string_list & operator<< (string_list&,char* str);

namespace Pleg
{
    class Server;

    extern boost::uuids::nil_generator nil_gen;
    extern boost::uuids::string_generator string_gen;

    extern int retval;
    extern const char *nullstr;

    extern Server *main_server;
    //FIXME
}

#define Debug() drumlin::logger(std::cerr)
#define Critical() drumlin::logger(std::cerr) << "********"

#endif // PLEG_H
