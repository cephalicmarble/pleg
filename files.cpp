#include <pleg.h>
using namespace Pleg;
#include "tao/json.hpp"
using namespace tao;
#include "files.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
using namespace boost;
using namespace boost::filesystem;
#include "writer.h"
#include "jsonconfig.h"

namespace Pleg {

recursive_mutex files_mutex;

void RecordedRange::toJson(json::value *json)const
{
    json::object_t &obj(json->get_object());
    json::value rel(json::empty_object);
    relevance.toJson(&rel);
    obj.insert({"relevance",rel});
    obj.insert({"start",lexical_cast<string>(start)});
    obj.insert({"finish",lexical_cast<string>(finish)});
    obj.insert({"step",step});
    obj.insert({"ticks",ticks});
}

bool operator ==(RecordedRange const& range,Relevance const& rel)
{
    return range.relevance == rel;
}

void Format::getStatus(json::value *status)const
{
    lock_guard l(&files_mutex);

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
    lock_guard l(&files_mutex);

    json::object_t &obj(json->get_object());
    getStatus(json);

    json::value rec(json::empty_object);
    json::object_t &_records(rec.get_object());
    int i=0;
    for(RecordedRange const& range : records){
        json::value _range(json::empty_object);
        range.toJson(&_range);
        _records.insert({tring(++i).toStdString(),_range});
    }
    obj.insert({"records",rec});
}

RecordedRange *Format::newRecord(const Buffers::Buffer *buffer)
{
    lock_guard l(&files_mutex);
    records.push_front(RecordedRange(buffer->getRelevanceRef(),buffer->getTimestampRef()));
    RecordedRange *range(&records.front());
    current_records.push_back(range);
    return range;
}

void Format::recordBuffer(const Buffers::Buffer *buffer)
{
    lock_guard l(&files_mutex);
    gint64 then(buffer->getTimestampRef());
    gint64 last(0);
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
        if(record->finish + 2*record->step < then){
            current_records.remove(record);
            record = newRecord(buffer);
        }
    }
    record->step = then - last;
    record->finish = then;
    record->ticks++;
}

Files::~Files()
{
    Files::writers_vec_type writers(list());
    for(auto file : writers){
        file->file.flush();
        delete file;
    }
}

Files::writers_vec_type Files::list()const
{
    writers_vec_type writers;
    for(auto file : what){
        if(writers.end()==std::find(writers.begin(),writers.end(),file.second)){
            writers.push_back(file.second);
        }
    }
    return writers;
}

void Files::add(Relevance const& rel,string const& filename)
{
    lock_guard l(&files_mutex);
    string path(Files::virtualFilePath(filename));
    if(!path.length()) {
        qLog() << "Attempted to access illegal path.";
        return;
    }
    FileWriter *file(getFileWriter(path));
    if(!file){
        file = new FileWriter(path);
        what.insert({path,file});
    }
    file->format.pattern.push_back(rel);//TODO add to record range
    std::pair<const Relevance,Buffers::Acceptor*> arg(std::make_pair(rel,file));
    Buffers::Cache(CPS_call_void(Buffers::registerRelevance,arg));
}

void Files::remove(string const& filename)
{
    lock_guard l(&files_mutex);
    string path(Files::virtualFilePath(filename));
    if(!path.length()) {
        qLog() << "Attempted to access illegal path.";
        return;
    }
    FileWriter *file(getFileWriter(filename));
    if(!file){
        return;
    }
    Buffers::Acceptor *acceptor(dynamic_cast<Buffers::Acceptor*>(file));
    Buffers::Cache(CPS_call_void(Buffers::unregisterAcceptor,acceptor));
}

FileWriter *Files::getFileWriter(string const& filename)
{
    what_type::iterator it(what.find(filename));
    return it==what.end()?nullptr:(*it).second;
}

} // namespace Pleg

namespace Files {

Files files;

string virtualFilePath(string filepath,bool isDirectory)
{
    Config::JsonConfig config(Config::files_config_file);
    string root(config["/files_root"].get_string().c_str()),absolutePath;
    filesystem::path rootdir(root);
    filesystem::path file(filepath);
    if(!filesystem::exists(rootdir) || !filesystem::exists(filepath)){
        return "";
    }
    absolutePath = filesystem::absolute(file,filesystem::initial_path()).string();
    if(0==absolutePath.find_first_of(filesystem::absolute(rootdir,filesystem::initial_path()))){
        return absolutePath;
    }
    return "";
}

void getStatus(json::value *status)
{
    json::value array(json::empty_array);
    for(Files::what_type::value_type &pair : files.what){
        array.get_array().push_back(pair.first.toStdString());
    }
    status->get_object().insert({"files",array});
}

string treeAt(json::value *tree,string path)
{
    filesystem::path p(path);
    if(!filesystem::exists(p) || !filesystem::is_directory(p))
        return "Directory not found: "+path;
    filesystem::directory_iterator iter(p);
    for (directory_entry& file : directory_iterator(p)) {
        if(file.path().string().substr(0,1)==".")
            continue;
        ofstream stat(file.path().string().c_str());
        if(!stat.is_open())
            continue;
        json::value node(json::empty_object);
        string nodepath(filesystem::absolute(file.path(),filesystem::initial_path()));
        if(file.status().type() | directory_file){
            node.get_object().insert({"type","directory"});
            json::value children(json::empty_array);
            string error = treeAt(&children,nodepath);
            if(error.length()){
                tree->get_array().clear();
                tree->get_array().push_back(error.toStdString());
                continue;
            }
            node.get_object().insert({"children",children});
        }else if(file.status().type() | regular_file){
            node.get_object().insert({"type","file"});
            node.get_object().insert({"size",filesystem::file_size(file.path())});
        }else{
            continue;
        }
        node.get_object().insert({"path",nodepath});
        node.get_object().insert({"name",file.path().filename().string()});
        tree->get_array().push_back(node);
    }
    return "";
}

}
