#include <pleg.h>
using namespace Pleg;
#include <vector>
#include "uri.h"
#include "relevance.h"
#include <sstream>
using namespace std;
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
using namespace boost;

namespace pleg {

/**
 * @brief UriParseFunc::UriParseFunc : Reads arguments from the URI definition string.
 * Arguments of the form {name?} will be accepted.
 * @param _pattern string
 */
UriParseFunc::UriParseFunc(const string _pattern)
{
    pattern = _pattern;
    int hunchback;
    if((hunchback = pattern.find_last_of('?') > pattern.find_last_of('}'))){ //have querystring
        vector<string> queries;
        algorithm::split(queries,pattern.substr(hunchback+1),is_any_of(","),algorithm::token_compress_on);
        for(string const& str : queries){
            query.push_back(str);
        }
    }else{
        hunchback = -1;
    }
    vector<string> pat_parts;
    algorithm::split(pat_parts,hunchback>=0?pattern.mid(0,hunchback):pattern,is_any_of("/"),algorithm::token_compress_on);
    for(string const& part : pat_parts){
        if(algorithm::find_head(part,1)=="{" && algorithm::find_tail(part,2)=="?}") {
            parts.push_back("{?}");
            params.push_back(part.substr(1,part.length()-3));
        }else if(algorithm::find_head(part,1)=="{" && algorithm::find_tail(part,1)=="}") {
            parts.push_back("{}");
            params.push_back(part.substr(1,part.length()-2));
        }else{
            parts.push_back(part);
        }
    }
    numOptionalParts = std::count_if(parts.begin(),parts.end(),[](string const& part){
        return part == "{?}";
    });
    numMandatoryParts = std::count_if(parts.begin(),parts.end(),[](string const& part){
        return part != "{?}";
    });
}

/**
 * @brief QSUriParseFunc::operator () : Matches them to url path items.
 * @param url QString
 * @return QSRelevance
 */
Relevance UriParseFunc::operator()(const string &url)const
{
    int hunchback;
    arguments_type query_params;
    if((hunchback = url.find_first_of('?'))!=string::npos){
        if(std::distance(query.begin(),query.end())==0)
            return false;
        params_type spec_params;
        std::copy(query.begin(),query.end(),std::back_inserter(spec_params));
        vector<string> pairs;
        algorithm::split(pairs,url.mid(hunchback+1),is_any_of("&"),algorithm::token_compress_on);
        for(string const& pair : pairs){
            vector<string> nv;
            algorithm::split(nv,pair,is_any_of("="),algorithm::token_compress_on);
            params_type::iterator end(spec_params.end());
            spec_params.erase(std::remove_if(spec_params.begin(),spec_params.end(),[nv](string const& q){
                regex rx("-(\\w+)");
                cmatch cap;
                if(regex_match(q,cap,rx)){
                    return cap[1] == nv[0];
                }else{
                    return q == nv[0];
                }
            }));
            if(end == spec_params.end())
                return false;
            if(std::distance(nv.begin(),nv.end())>1){
                query_params.insert({nv[0],nv[1]});
            }
        }
        for(string const& param : spec_params){
            if(string::npos == param.find('-'))
                return false;
        }
    }
    vector<string> pat_parts;
    algorithm::split(pat_parts,(hunchback>=0?url.mid(0,hunchback):url),is_any_of("/"),algorithm::token_compress_on);
    auto count(std::distance(pat_parts.begin(),pat_parts.end()));
    if(count > numMandatoryParts + numOptionalParts)
        return false;
    if(count < numMandatoryParts)
        return false;
    params_type::const_iterator part_it = parts.begin();
    arguments_type arguments;
    quint8 i(0);
    for(string &part : pat_parts){
        if(*part_it == "{?}" || *part_it == "{}"){
            if(part.length() && i<std::distance(params.begin(),params.end())){
                arguments.insert(arguments.end(),{params.at(i),part});
            }else{
                arguments.insert(arguments.end(),{lexical_cast<string>(i),part});
            }
            i++;
        }else{
            if(part != *part_it){
                return false;
            }
            part_it = std::next(part_it);
        }
    }
    Relevance rel;
    if(!std::distance(arguments.begin(),arguments.end()))
        rel = true;
    else
        rel = Relevance::fromArguments(arguments,std::distance(params.begin(),params.end())==i && i>0);
    rel.setQueryParams(query_params);
    return rel;
}

namespace Uri {

/**
 * @brief parser : convenience namespace and function
 * @param _pattern QString
 * @return QSUriParseFunc
 */
QSUriParseFunc parser(const QString _pattern)
{
    return QSUriParseFunc(_pattern);
}

/**
 * @brief parser : convenience namespace and function
 * @param _pattern const char *
 * @return QSUriParseFunc
 */
QSUriParseFunc parser(const char *_pattern)
{
    return QSUriParseFunc(QString(_pattern));
}

}

} // namespace pleg
