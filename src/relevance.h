#ifndef RELEVANCE_H
#define RELEVANCE_H

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>
using namespace boost;
#include <iostream>
using namespace std;
#include "drumlin/event.h"
using namespace drumlin;
#include "uri.h"

namespace Pleg {

namespace Sources {
    class Source;
}

/**
 * @brief The Relevance class : for assessing relevance of samples, streams, transforms etc
 */
class Relevance
{
public:
    bool argParity;
    bool queryParity;
    typedef UriParseFunc::arguments_type arguments_type;
    arguments_type arguments;
    arguments_type params;
    Relevance(bool _true = true);
    Relevance(Sources::Source *source);
    Relevance(const char* source_name);
    Relevance(const Relevance &&rel);
    Relevance(const Relevance &rhs);

    Sources::Source *getSource()const;
    void setSource(Pleg::Sources::Source *_source);
    bool hasSource()const{ return nullptr != source; }
    string getSourceName()const{ return source_name; }
    void setSourceName(string name){ source_name = name; }
    uuids::uuid getUuid()const;
    void setUuid(uuids::uuid uuid);
    void setUuid(string &uuid);
    void setQueryParams(arguments_type query_params);
    bool compare(const Relevance &rhs)const;

    void operator=(const char *source_name);
    void operator=(const Relevance &);
    bool operator<(const Relevance &)const;

    bool toBool()const;
    void toJson(json::value *object)const;

    static Relevance fromArguments(const UriParseFunc::arguments_type &arguments,bool argParity = false);
    static Relevance fromSource(const Pleg::Sources::Source *source);

    bool operator==(const Relevance &)const;
    friend logger &operator<<(logger &stream,const Relevance &rhs);
private:
    Pleg::Sources::Source *source = nullptr;
    string source_name = "";
    uuids::uuid uuid;
};

extern logger &operator<<(logger &stream,const Pleg::Relevance &rhs);
extern stringstream &operator<<(stringstream &strm,uuids::uuid const& uuid);
extern ostream &operator<<(ostream &strm,uuids::uuid const& uuid);
extern logger &operator<<(logger &strm,uuids::uuid const& uuid);

} // namespace Pleg

namespace drumlin {
template <>
PodEvent<Pleg::Relevance::arguments_type> *pod_event_cast(const Event *event,Pleg::Relevance::arguments_type *out);
}

#endif // RELEVANCE_H
