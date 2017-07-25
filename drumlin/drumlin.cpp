#include "drumlin.h"

#include <boost/algorithm/string.hpp>

namespace drumlin {

logger& operator<< (logger& strm, const boost::any& p)
{
    mpl::for_each<value_types>(stream_operator_impl(strm,p));
    return strm;
}

string_list &operator<< (string_list &vecS,string const& str)
{
    vecS.push_back(str);
    return vecS;
}

string_list &operator<< (string_list &vecS,string & str)
{
    vecS.push_back(str);
    return vecS;
}

string_list &operator<< (string_list &vecS,const char* str)
{
    vecS.push_back(string(str));
    return vecS;
}

string_list &operator<< (string_list &vecS,char* str)
{
    vecS.push_back(string(str));
    return vecS;
}

string string_list::join(string str)
{
    return algorithm::join(*this,str);
}

string string_list::join(const char*pc)
{
    return join(string(pc));
}

string_list string_list::fromString(string const& toSplit,const char* delim,bool all,algorithm::token_compress_mode_type flags)
{
    return string_list::fromString(toSplit,string(delim),all,flags);
}

string_list string_list::fromString(string const& toSplit,const char delim,bool all,algorithm::token_compress_mode_type flags)
{
    return string_list::fromString(toSplit,string(&delim),all,flags);
}

string_list string_list::fromString(string const& toSplit,string delim,bool all,algorithm::token_compress_mode_type flags)
{
    string_list list;
    if(!all){
        algorithm::split(list,toSplit,algorithm::is_any_of(delim),flags);
    }else{
        string::size_type pos(string::npos);
        string s(toSplit);
        while((pos=s.find(delim))!=string::npos)s.replace(pos,delim.length(),"¬");
        algorithm::split(list,s,algorithm::is_any_of("¬"),flags);
    }
    return list;
}

string_list &string_list::operator=(string_list const& rhs)
{
    std::copy(rhs.begin(),rhs.end(),back_inserter(*this));
    return *this;
}

}
