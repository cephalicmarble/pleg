#ifndef FILES_H
#define FILES_H

#include "buffer.h"
#include "relevance.h"
#include "source.h"
#include "object.h"
#include "glib.h"
#include <boost/date_time/posix_time/posix_time.hpp>
using namespace boost;

namespace Pleg {

class FileWriter;

class RecordedRange
{
public:
    Relevance relevance;
    gint64 start;
    gint64 step;
    gint64 finish;
    guint64 ticks;
    void toJson(json::value *json)const;
    //void fromJson(json::value *json);
    RecordedRange(Relevance const&rel,posix_time::ptime _start)
        :relevance(rel),start(_start),step(-9),finish(start),ticks(0)
    {
        if(rel.getSource()){
            step = rel.getSource()->getTau();
        }
    }
    RecordedRange(RecordedRange &rhs)
        :relevance(rhs.relevance),start(rhs.start),step(rhs.step),finish(rhs.finish),ticks(rhs.ticks){}
    RecordedRange(RecordedRange &&rhs)
        :relevance(rhs.relevance),start(rhs.start),step(rhs.step),finish(rhs.finish),ticks(rhs.ticks){}
    friend bool operator ==(RecordedRange const&,Relevance const&);
};
bool operator ==(RecordedRange const&,Relevance const&);

class Format
{
public:
    typedef std::vector<Relevance> pattern_type;
    pattern_type pattern;
    typedef std::list<RecordedRange> record_type;
    record_type records;
    std::list<RecordedRange*> current_records;
    void getStatus(json::value *status)const;
    void toJson(json::value *json)const;
    RecordedRange *newRecord(const Buffers::Buffer *buffer);
    void recordBuffer(const Buffers::Buffer *buffer);
//    void fromJson(json::value *json);
};

namespace Files{
void getStatus(json::value *status);
string treeAt(json::value *tree,tring root);
}

/**
 * @brief The FileWriter class : handles a number of files
 */
class Files
{
public:
    typedef std::map<string,FileWriter*> what_type;
    what_type what;
    typedef std::vector<FileWriter*> writers_vec_type;
    Files(){}
    virtual ~Files();
public:
    writers_vec_type list()const;
    void add(Relevance const& rel,string const& filename);
    void remove(tring const& filename);
    void addFromIndex(json::value *json);
    FileWriter *getFileWriter(string const& filename);
    friend void Files::getStatus(json::value *status);
};

namespace Files {
    extern Files files;
    string virtualFilePath(string filepath,bool isDirectory = false);
}

} // namespace Pleg

#endif // FILES_H
