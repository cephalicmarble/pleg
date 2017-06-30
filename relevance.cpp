#include <qs.h>
using namespace QS;
#include <tao/json.hh>
using namespace tao;
#include "relevance.h"
#include <QDebug>
#include "source.h"
#include <algorithm>
#include <string>
#include <iostream>
#include <sstream>
using namespace std;

/**
 * @brief QSRelevance::QSRelevance : the default QSRelevance is falsy
 * @param untrue
 */
QSRelevance::QSRelevance(bool _true):argParity(false),queryParity(false)
{
    if(_true){
        source = (Sources::QSSource*)1;
        uuid = QUuid::fromRfc4122("truthy");
    }else{
        source = nullptr;
        uuid = QUuid();
    }
}
/**
 * @brief QSRelevance::QSRelevance : construct from a given source
 * @param _source Sources::QSSource*
 */
QSRelevance::QSRelevance(Sources::QSSource *_source):argParity(false),queryParity(false)
{
    source = _source;
    if(!!_source){
        source_name = (const char*)*source;
        uuid = _source->getUuid();
    }else{
        source_name = "<empty>";
        uuid = QUuid();
    }
}

QSRelevance::QSRelevance(const char* _source_name):argParity(false),queryParity(false)
{
    operator=(_source_name);
}

/**
 * @brief QSRelevance::QSRelevance : move constructor
 * @param rel QSRelevance const&&
 */
QSRelevance::QSRelevance(const QSRelevance &&rel)
{
    operator =(std::move(rel));
}

/**
 * @brief QSRelevance::QSRelevance : copy constructor
 * @param rhs QSRelevance const&
 */
QSRelevance::QSRelevance(const QSRelevance &rhs)
{
    operator =(rhs);
}
/**
 * @brief QSRelevance::getSource
 * @return Sources::QSSource*
 */
Sources::QSSource *QSRelevance::getSource()const
{
    return source;
}

/**
 * @brief QSRelevance::setSource : setSource for the relevance
 * @param _source Sources::QSSource*
 */
void QSRelevance::setSource(Sources::QSSource *_source)
{
    source = _source;
    source_name = (const char*)*_source;
    uuid = _source->getUuid();
}

/**
 * @brief QSRelevance::getUuid
 * @return QUuid
 */
QUuid QSRelevance::getUuid()const
{
    return uuid;
}

/**
 * @brief QSRelevance::setUuid
 * @param _uuid QSRelevance
 */
void QSRelevance::setUuid(QUuid _uuid)
{
    uuid = _uuid;
}

void QSRelevance::setQueryParams(arguments_type query_params)
{
    params.clear();
    std::copy(query_params.begin(),query_params.end(),std::inserter(params,params.begin()));
}

/**
 * @brief QSRelevance::compare : assess equality to the rhs
 * @param rhs QSRelevance const&
 * @return bool
 */
bool QSRelevance::compare(const QSRelevance &rhs)const
{
    return source_name == rhs.source_name;
}

void QSRelevance::operator=(const char *rhs)
{
    source = Sources::sources.fromString<Sources::QSSource>(rhs);
    if(!!source){
        source_name = rhs;
        uuid = source->getUuid();
    }else{
        uuid = QUuid::fromRfc4122("any experiment");
    }
}

/**
 * @brief QSRelevance::operator = : copy assignment
 * @param rhs QSRelevance const&
 */
void QSRelevance::operator=(const QSRelevance &rhs)
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
 * @brief QSRelevance::operator bool : assess constant truthiness
 */
bool QSRelevance::toBool()const
{
    return nullptr != source || !uuid.isNull() || argParity || queryParity;
}

void QSRelevance::toJson(json::value *object)const
{
    json::object_t &obj(object->get_object());
    obj.insert({"source",source_name});
    obj.insert({"uuid",uuid.toString().toStdString()});
}

/**
 * @brief operator const char*: for QDebug
 */
QSRelevance::operator const char*()const
{
    std::stringstream ss;
    ss << *this;
    return strdup(ss.str().c_str());
}

namespace Relevance {

/**
 * @brief fromArguments : derive Relevance from parsed URI
 * @param c_arguments QSUriParseFunc::arguments_type
 * @return QSRelevance
 */
QSRelevance fromArguments(const QSUriParseFunc::arguments_type &c_arguments,bool argParity)
{
    typedef QSUriParseFunc::arguments_type arguments_type;
    arguments_type &arguments = const_cast<arguments_type&>(c_arguments);
    QSRelevance ret("");
    if(arguments.end()!=arguments.find("source")){
        ret = arguments.at("source").toStdString().c_str();
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
 * @param source Sources::QSSource*
 * @return QSRelevance
 */
QSRelevance fromSource(const Sources::QSSource *source)
{
    QSRelevance rel(source);
    return rel;
}

}

/**
 * @brief operator == : c++ technique
 * @param lhs QSRelevance const&
 * @param rhs QSRelevance const&
 * @return bool
 */
bool operator==(const QSRelevance &lhs,const QSRelevance &rhs)
{
    return lhs.toBool() && rhs.toBool() && lhs.compare(rhs);
}

/**
 * @brief operator << : stream operator
 * @param stream std::ostream &
 * @param rhs QSRelevance const&
 * @return std::ostream &
 */
std::ostream &operator<<(std::ostream &stream,const QSRelevance &rel)
{
    if(!rel.source){
        stream << " relevance is irrelevant (tm)";
        return stream;
    }
    stream << &rel
           << " - name:" << rel.source_name
           << " - uuid:" << rel.uuid.toString().toStdString();
    return stream;
}

/**
 * @brief operator << : stream operator
 * @param stream QDebug&
 * @param rhs QRelevance const&
 * @return QDebug&
 */
QDebug &operator<<(QDebug &stream,const QSRelevance &rhs)
{
    stream << QString(rhs);
    return stream;
}

template <>
QSPodEvent<QSRelevance::arguments_type> *pod_event_cast(const QSEvent *event,QSRelevance::arguments_type *out)
{
    if(out){
        QSPodEvent<QSRelevance::arguments_type> *ret = static_cast<QSPodEvent<QSRelevance::arguments_type>*>(const_cast<QSEvent*>(event));
        if(0==std::distance(out->begin(),out->end())){
            for(auto item : ret->getVal()){
                out->insert(item);
            }
        }
        return ret;
    }
    return static_cast<QSPodEvent<QSRelevance::arguments_type>*>(const_cast<QSEvent*>(event));
}
