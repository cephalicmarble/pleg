#ifndef URI_H
#define URI_H

#include "object.h"
#include <vector>

namespace Pleg {

class Relevance;
/**
 * @brief The QSUriParseFunc struct
 *
 * Reads arguments from the URI definition string.
 * Matches them to URL path items.
 * Creates QSRelevance from the arguments_type map.
 */
struct UriParseFunc {
    string pattern;
    typedef std::vector<string> params_type;
    params_type params;
    params_type parts;
    params_type query;
    int numOptionalParts;
    int numMandatoryParts;
    typedef std::map<string,string> arguments_type;
    Relevance operator()(const string &uri)const;
    UriParseFunc(const string _pattern);
    int paramsCount(){return std::distance(params.begin(),params.end()); }
};

namespace Uri {

UriParseFunc parser(const string _pattern);
UriParseFunc parser(const char *_pattern);

}

} // namespace pleg

#endif // URI_H
