#ifndef QSRELEVANCE_H
#define QSRELEVANCE_H

#include <boost/uuid/uuid.hpp>
#include <iostream>
#include "uri.h"
#include "event.h"

namespace Sources {
    class QSSource;
}

/**
 * @brief The QSRelevance class : for assessing relevance of samples, streams, transforms etc
 */
class QSRelevance
{
    Sources::QSSource *source = nullptr;
    std::string source_name = "";
    QUuid uuid;
public:
    bool argParity;
    bool queryParity;
    typedef QSUriParseFunc::arguments_type arguments_type;
    arguments_type arguments;
    arguments_type params;
    QSRelevance(bool _true = true);
    QSRelevance(Sources::QSSource *source);
    QSRelevance(const char* source_name);
    QSRelevance(const QSRelevance &&rel);
    QSRelevance(const QSRelevance &rhs);

    Sources::QSSource *getSource()const;
    void setSource(Sources::QSSource *_source);
    bool hasSource()const{ return nullptr != source; }
    std::string getSourceName()const{ return source_name; }
    void setSourceName(std::string name){ source_name = name; }
    QUuid getUuid()const;
    void setUuid(QUuid uuid);
    void setQueryParams(arguments_type query_params);
    bool compare(const QSRelevance &rhs)const;

    void operator=(const char *source_name);
    void operator=(const QSRelevance &);

    bool toBool()const;
    void toJson(json::value *object)const;
    operator const char*()const;

    friend bool operator==(const QSRelevance &,const QSRelevance &);
    friend std::ostream &operator<<(std::ostream &stream,const QSRelevance &rhs);
    friend QDebug &operator<<(QDebug &stream,const QSRelevance &rhs);
};

namespace Relevance {
    QSRelevance fromArguments(const QSUriParseFunc::arguments_type &arguments,bool argParity = false);
    QSRelevance fromSource(const Sources::QSSource *source);
}

bool operator==(const QSRelevance &,const QSRelevance &);
std::ostream &operator<<(std::ostream &stream,const QSRelevance &rhs);
QDebug &operator<<(QDebug &stream,const QSRelevance &rhs);

template <>
QSPodEvent<QSRelevance::arguments_type> *pod_event_cast(const QSEvent *event,QSRelevance::arguments_type *out);

#endif // QSRELEVANCE_H
