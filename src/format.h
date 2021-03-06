#ifndef FORMAT_H
#define FORMAT_H

#include <boost/date_time/posix_time/posix_time.hpp>
using namespace boost;
#include "relevance.h"
#include "drumlin/tao_forward.h"
using namespace tao;
#include "drumlin/gtypes.h"
#include "drumlin/object.h"
#include "source.h"

namespace Pleg {

namespace Files {

class FileWriter;
class Files;
void getStatus(json::value *status);

extern std::recursive_mutex files_mutex;
#define FILESLOCK  std::lock_guard<std::recursive_mutex> l(const_cast<std::recursive_mutex&>(files_mutex))

class RecordedRange
{
public:
    Relevance relevance;
    posix_time::ptime start;
    posix_time::ptime::time_duration_type step;
    posix_time::ptime finish;
    guint64 ticks;
    void toJson(json::value *json)const;
    //void fromJson(json::value *json);
    RecordedRange(Relevance const&rel,posix_time::ptime _start);
    RecordedRange(RecordedRange &rhs);
    RecordedRange(RecordedRange &&rhs);
    friend bool operator ==(RecordedRange const&,Relevance const&);
};
bool operator ==(RecordedRange const&,Relevance const&);

class Format
{
public:
    typedef vector<Relevance> pattern_type;
    pattern_type pattern;
    typedef list<RecordedRange> record_type;
    record_type records;
    list<RecordedRange*> current_records;
    void getStatus(json::value *status)const;
    void toJson(json::value *json)const;
    RecordedRange *newRecord(const Buffers::Buffer *buffer);
    void recordBuffer(const Buffers::Buffer *buffer);
//    void fromJson(json::value *json);
};

} // namespace Files

} // namespace Pleg

#endif // FORMAT_H
