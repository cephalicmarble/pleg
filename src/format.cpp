#define TAOJSON
#include "format.h"

#include <mutex>
using namespace std;
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
using namespace boost;
using namespace boost::filesystem;
#include "drumlin/jsonconfig.h"
using namespace drumlin;
#include "writer.h"
#include "log.h"
#include "pleg.h"
using namespace Pleg;

namespace Pleg {

namespace Files {

std::recursive_mutex files_mutex;

RecordedRange::RecordedRange(Relevance const&rel,posix_time::ptime _start)
    :relevance(rel),start(_start),step(posix_time::milliseconds(0)),finish(start),ticks(0)
{
    if(rel.getSource()){
        step = posix_time::milliseconds(rel.getSource()->getTau());
    }
}

RecordedRange::RecordedRange(RecordedRange &rhs)
    :relevance(rhs.relevance),start(rhs.start),step(rhs.step),finish(rhs.finish),ticks(rhs.ticks)
{

}

RecordedRange::RecordedRange(RecordedRange &&rhs)
    :relevance(rhs.relevance),start(rhs.start),step(rhs.step),finish(rhs.finish),ticks(rhs.ticks)
{

}

void RecordedRange::toJson(json::value *json)const
{
    json::object_t &obj(json->get_object());
    json::value rel(json::empty_object);
    relevance.toJson(&rel);
    obj.insert({"relevance",rel});
    obj.insert({"start",lexical_cast<string>(start)});
    obj.insert({"finish",lexical_cast<string>(finish)});
    obj.insert({"step",posix_time::to_simple_string(step)});
    obj.insert({"ticks",ticks});
}

bool operator ==(RecordedRange const& range,Relevance const& rel)
{
    return range.relevance == rel;
}

void Format::getStatus(json::value *status)const
{
    FILESLOCK;

    json::object_t &obj(status->get_object());

    json::value pat(json::empty_object);
    json::object_t &patterns(pat.get_object());
    for(Relevance const& rel : pattern){
        json::value _rel(json::empty_object);
        rel.toJson(&_rel);
        patterns.insert({rel.getSourceName(),_rel});
    }
    obj.insert({"pattern",pat});

    json::value cur(json::empty_object);
    json::object_t &working(cur.get_object());
    for(RecordedRange const* range : current_records){
        json::value _range(json::empty_object);
        range->toJson(&_range);
        working.insert({range->relevance.getSourceName(),_range});
    }
    obj.insert({"working",cur});
}

void Format::toJson(json::value *json)const
{
    FILESLOCK;

    json::object_t &obj(json->get_object());
    getStatus(json);

    json::value rec(json::empty_object);
    json::object_t &_records(rec.get_object());
    int i=0;
    for(RecordedRange const& range : records){
        json::value _range(json::empty_object);
        range.toJson(&_range);
        _records.insert({lexical_cast<string>(++i),_range});
    }
    obj.insert({"records",rec});
}

RecordedRange *Format::newRecord(const Buffers::Buffer *buffer)
{
    std::lock_guard<std::recursive_mutex> l(files_mutex);
    records.push_front(RecordedRange(buffer->getRelevanceRef(),buffer->getTimestampRef()));
    RecordedRange *range(&records.front());
    current_records.push_back(range);
    return range;
}

void Format::recordBuffer(const Buffers::Buffer *buffer)
{
    FILESLOCK;
    posix_time::ptime then(buffer->getTimestampRef());
    posix_time::ptime last;
    if(pattern.end()==std::find(pattern.begin(),pattern.end(),buffer->getRelevanceRef())){
        throw new Exception("Irrelevant buffer to record");
    } //TODO remove sanity check
    record_type::iterator record_it(std::find_if(records.begin(),records.end(),[buffer](RecordedRange const& range){
        return range == buffer->getRelevanceRef();
    }));
    RecordedRange *record;
    if(record_it == records.end()) {
        record = newRecord(buffer);
    }else{
        record = &(*record_it);
        last = record->finish;
        if(record->finish + record->step*2 < then){
            current_records.remove(record);
            record = newRecord(buffer);
        }
    }
    record->step = then - last;
    record->finish = then;
    record->ticks++;
}

} // namespace Files

} // namespace Pleg
