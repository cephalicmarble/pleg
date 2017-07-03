#ifndef RELEVANCE_H
#define RELEVANCE_H

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/string_generator.hpp>
using namespace boost;
#include <iostream>
using namespace std;
#include "uri.h"
#include "event.h"
using namespace drumlin;

namespace Sources {
    class Source;
}

namespace Pleg {

/**
 * @brief The Relevance class : for assessing relevance of samples, streams, transforms etc
 */
class Relevance
{
    Sources::Source *source = nullptr;
    string source_name = "";
    uuids::uuid uuid;
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
    void setSource(Sources::Source *_source);
    bool hasSource()const{ return nullptr != source; }
    string getSourceName()const{ return source_name; }
    void setSourceName(string name){ source_name = name; }
    uuids::uuid getUuid()const;
    void setUuid(uuids::uuid uuid);
    void setQueryParams(arguments_type query_params);
    bool compare(const Relevance &rhs)const;

    void operator=(const char *source_name);
    void operator=(const Relevance &);

    bool toBool()const;
    void toJson(json::value *object)const;
    operator const char*()const;

    friend bool operator==(const Relevance &,const Relevance &);
    friend ostream &operator<<(ostream &stream,const Relevance &rhs);
};

namespace Relevance {
    Relevance fromArguments(const UriParseFunc::arguments_type &arguments,bool argParity = false);
    Relevance fromSource(const Sources::Source *source);
}

bool operator==(const Relevance &,const Relevance &);
ostream &operator<<(ostream &stream,const Relevance &rhs);

template <>
PodEvent<Relevance::arguments_type> *pod_event_cast(const Event *event,Relevance::arguments_type *out);

#endif // RELEVANCE_H
