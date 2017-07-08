#ifndef URI_H
#define URI_H

#include <vector>
using namespace std;
#include "object.h"
#include "socket.h"
using namespace drumlin;

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
    typedef string_list params_type;
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

extern UriParseFunc parser(const string _pattern);
extern UriParseFunc parser(const char *_pattern);

}

} // namespace pleg

#endif // URI_H
