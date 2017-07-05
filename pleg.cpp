#include "pleg.h"

ostream& operator<< (ostream& strm, const boost::any& p)
{
    mpl::for_each<value_types>(stream_operator_impl(strm,p));
    return strm;
}

vector<string>& operator<< (vector<string>& vecS,string const& str)
{
    vecS.push_back(str);
    return vecS;
}

ostream& operator<< (ostream &strm, const byte_array &bytes)
{
    strm << bytes.string();
    return strm;
}

namespace Pleg {

extern boost::uuids::string_generator string_gen;

}
