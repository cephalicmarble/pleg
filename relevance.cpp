#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include "relevance.h"
#include "source.h"
#include <algorithm>
#include <string>
#include <iostream>
#include <sstream>
using namespace std;
#include <boost/lexical_cast.hpp>
using namespace boost;

namespace Pleg {

/**
 * @brief Relevance::Relevance : the default Relevance is falsy
 * @param untrue
 */
Relevance::Relevance(bool _true):argParity(false),queryParity(false)
{
    if(_true){
        source = (Sources::Source*)1;
        uuid = string_gen("ffffffffffffffffffffffffffffffff");
    }else{
        source = nullptr;
        uuid = string_gen("11111111111111111111111111111111");
    }
}
/**
 * @brief Relevance::Relevance : construct from a given source
 * @param _source Sources::Source*
 */
Relevance::Relevance(Sources::Source *_source):argParity(false),queryParity(false)
{
    source = _source;
    if(!!_source){
        source_name = (const char*)*source;
        uuid = _source->getUuid();
    }else{
        source_name = "<empty>";
        uuid = string_gen("00000000000000000000000000000000");
    }
}

Relevance::Relevance(const char* _source_name):argParity(false),queryParity(false)
{
    operator=(_source_name);
}

/**
 * @brief Relevance::Relevance : move constructor
 * @param rel Relevance const&&
 */
Relevance::Relevance(const Relevance &&rel)
{
    operator =(std::move(rel));
}

/**
 * @brief Relevance::Relevance : copy constructor
 * @param rhs Relevance const&
 */
Relevance::Relevance(const Relevance &rhs)
{
    operator =(rhs);
}
/**
 * @brief Relevance::getSource
 * @return Sources::Source*
 */
Sources::Source *Relevance::getSource()const
{
    return source;
}

/**
 * @brief Relevance::setSource : setSource for the relevance
 * @param _source Sources::Source*
 */
void Relevance::setSource(Sources::Source *_source)
{
    source = _source;
    source_name = (const char*)*_source;
    uuid = _source->getUuid();
}

/**
 * @brief Relevance::getUuid
 * @return QUuid
 */
uuids::uuid Relevance::getUuid()const
{
    return uuid;
}

/**
 * @brief Relevance::setUuid
 * @param _uuid Relevance
 */
void Relevance::setUuid(uuids::uuid _uuid)
{
    uuid = _uuid;
}

/**
 * @brief Relevance::setUuid
 * @param string uuid
 */
void Relevance::setUuid(string &_uuid)
{
    uuid = string_gen(_uuid);
}

void Relevance::setQueryParams(arguments_type query_params)
{
    params.clear();
    std::copy(query_params.begin(),query_params.end(),std::inserter(params,params.begin()));
}

/**
 * @brief Relevance::compare : assess equality to the rhs
 * @param rhs Relevance const&
 * @return bool
 */
bool Relevance::compare(const Relevance &rhs)const
{
    return source_name == rhs.source_name;
}

bool Relevance::operator<(const Relevance &rhs)const
{
    if(*this==rhs)
        return false;
    return source_name < rhs.source_name;
}

void Relevance::operator=(const char *rhs)
{
    source = Sources::sources.fromString<Sources::Source>(rhs);
    if(!!source){
        source_name = rhs;
        uuid = source->getUuid();
    }else{
        uuid = string_gen("0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
    }
}

/**
 * @brief Relevance::operator = : copy assignment
 * @param rhs Relevance const&
 */
void Relevance::operator=(const Relevance &rhs)
{
    source = rhs.source;
    source_name = rhs.source_name;
    uuid = rhs.uuid;
    argParity = rhs.argParity;
    queryParity = rhs.queryParity;
    arguments.clear();
    std::copy(rhs.arguments.begin(),rhs.arguments.end(),std::inserter(arguments,arguments.begin()));
    params.clear();
    std::copy(rhs.params.begin(),rhs.params.end(),std::inserter(params,params.begin()));
}

/**
 * @brief Relevance::operator bool : assess constant truthiness
 */
bool Relevance::toBool()const
{
    return nullptr != source || !uuid.is_nil() || argParity || queryParity;
}

void Relevance::toJson(json::value *object)const
{
    json::object_t &obj(object->get_object());
    obj.insert({"source",source_name});
    stringstream ss;
    ss << uuid;
    obj.insert({"uuid",ss.str()});
}

/**
 * @brief fromArguments : derive Relevance from parsed URI
 * @param c_arguments UriParseFunc::arguments_type
 * @return Relevance
 */
Relevance Relevance::fromArguments(const UriParseFunc::arguments_type &c_arguments,bool argParity)
{
    typedef UriParseFunc::arguments_type arguments_type;
    arguments_type &arguments = const_cast<arguments_type&>(c_arguments);
    Relevance ret("");
    if(arguments.end()!=arguments.find("source")){
        ret = arguments.at("source").c_str();
    }
    if(arguments.end()!=arguments.find("uuid")){
        ret.setUuid(arguments.at("uuid"));
    }
    for(auto arg : arguments){
        ret.arguments.insert(arg);
    }
    ret.argParity = argParity;
    return ret;
}

/**
 * @brief fromSource : utility function for assessing source relevance
 * @param source Sources::Source*
 * @return Relevance
 */
Relevance Relevance::fromSource(const Sources::Source *source)
{
    Relevance rel(source);
    return rel;
}

/**
 * @brief operator == : c++ technique
 * @param lhs Relevance const&
 * @param rhs Relevance const&
 * @return bool
 */
bool Relevance::operator==(const Pleg::Relevance &rhs)const
{
    return toBool() && rhs.toBool() && compare(rhs);
}

/**
 * @brief operator << : stream operator
 * @param stream std::ostream &
 * @param rhs Relevance const&
 * @return std::ostream &
 */
logger &operator<<(logger &stream,const Pleg::Relevance &rel)
{
    if(!rel.source){
        stream << " relevance is irrelevant (tm)";
        return stream;
    }
    stream << "{"
           << "name:" << rel.source_name
           << "uuid:" << rel.uuid
           << "}";
    return stream;
}

logger &operator<<(logger &strm,uuids::uuid const& uuid)
{
    strm << "0x";
    for(int i=0;i<16;i++){
        strm << uuid.data[i];
    }
    return strm;
}

stringstream &operator<<(stringstream &strm,uuids::uuid const& uuid)
{
    strm << "0x";
    for(int i=0;i<16;i++){
        strm << uuid.data[i];
    }
    return strm;
}

ostream &operator<<(ostream &strm,uuids::uuid const& uuid)
{
    strm << "0x";
    for(int i=0;i<16;i++){
        strm << uuid.data[i];
    }
    return strm;
}

} // namespace Pleg

namespace drumlin {

template <>
PodEvent<Pleg::Relevance::arguments_type> *pod_event_cast(const Event *event,Pleg::Relevance::arguments_type *out)
{
    if(out){
        PodEvent<Relevance::arguments_type> *ret = static_cast<PodEvent<Relevance::arguments_type>*>(const_cast<Event*>(event));
        if(0==std::distance(out->begin(),out->end())){
            for(auto item : ret->getVal()){
                out->insert(item);
            }
        }
        return ret;
    }
    return static_cast<PodEvent<Relevance::arguments_type>*>(const_cast<Event*>(event));
}

} // namespace drumlin
