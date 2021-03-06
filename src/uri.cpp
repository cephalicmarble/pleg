#include "uri.h"

#include <vector>
#include <sstream>
using namespace std;
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
using namespace boost;
#include "pleg.h"
#include "relevance.h"
using namespace Pleg;

namespace Pleg {

/**
 * @brief UriParseFunc::UriParseFunc : Reads arguments from the URI definition string.
 * Arguments of the form {name?} will be accepted.
 * @param _pattern string
 */
UriParseFunc::UriParseFunc(const string _pattern)
{
    pattern = _pattern;
    string::size_type hunchback;
    if((hunchback = pattern.find_last_of('?')) > pattern.find_last_of('}')
    || (string::npos==pattern.find_last_of('}') && string::npos!=hunchback))
    { //have querystring
        vector<string> queries;
        string haystack(pattern.substr(hunchback+1));
        algorithm::split(queries,haystack,algorithm::is_any_of(","),algorithm::token_compress_on);
        for(string const& str : queries){
            query.push_back(str);
        }
    }else{
        hunchback = string::npos;
    }
    vector<string> pat_parts;
    string mid(hunchback!=string::npos?pattern.substr(0,hunchback):pattern);
    algorithm::trim_if(mid,algorithm::is_any_of(" /"));
    algorithm::split(pat_parts,mid,algorithm::is_any_of("/"),algorithm::token_compress_on);
    for(string const& part : pat_parts){
        if(algorithm::starts_with(part,"{") && algorithm::ends_with(part,"?}")) {
            parts.push_back("{?}");
            params.push_back(part.substr(1,part.length()-3));
        }else if(algorithm::starts_with(part,"{") && algorithm::ends_with(part,"}")) {
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
 * @brief UriParseFunc::operator () : Matches them to url path items.
 * @param url string
 * @return Relevance
 */
Relevance UriParseFunc::operator()(const string &url)const
{
    string::size_type hunchback;
    arguments_type query_params;
    params_type spec_params;
    std::copy(query.begin(),query.end(),std::back_inserter(spec_params));
    if((hunchback = url.find_first_of('?'))!=string::npos){
        if(std::distance(query.begin(),query.end())==0)
            return false;
        vector<string> pairs;
        string mid(url.substr(hunchback+1));
        algorithm::split(pairs,mid,algorithm::is_any_of("&"),algorithm::token_compress_on);
        for(string const& pair : pairs){
            vector<string> nv;
            algorithm::split(nv,pair,algorithm::is_any_of("="),algorithm::token_compress_on);
            params_type::iterator end(spec_params.end());
            spec_params.erase(std::remove_if(spec_params.begin(),spec_params.end(),[nv](string & q){
                regex rx("-(\\w+)");
                smatch cap;
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
    }
    if(std::distance(spec_params.begin(),spec_params.end())){
        for(string & param : spec_params){
            string::size_type pos = param.find('-');
            if(string::npos == pos){
                return false;
            }else{
                query_params.insert({param.replace(pos,1,""),""});
            }
        }
    }
    string mid(hunchback!=string::npos?url.substr(0,hunchback):url);
    algorithm::trim_if(mid,algorithm::is_any_of(" /"));
    vector<string> pat_parts;
    algorithm::split(pat_parts,mid,algorithm::is_any_of("/"),algorithm::token_compress_on);
    auto count(std::distance(pat_parts.begin(),pat_parts.end()));
    if(count > numMandatoryParts + numOptionalParts)
        return false;
    if(count < numMandatoryParts)
        return false;
    params_type::const_iterator part_it = parts.begin();
    arguments_type arguments;
    guint8 i(0);
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
 * @param _pattern string
 * @return UriParseFunc
 */
UriParseFunc parser(const string &_pattern)
{
    return UriParseFunc(_pattern);
}

/**
 * @brief parser : convenience namespace and function
 * @param _pattern const char *
 * @return UriParseFunc
 */
UriParseFunc parser(const char *_pattern)
{
    string s(_pattern);
    return UriParseFunc(s);
}

}

} // namespace pleg
