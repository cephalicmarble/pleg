#include <pleg.h>
using namespace Pleg;
#include "tao/json.hpp"
using namespace tao;
#include "files.h"
#include <mutex>
using namespace std;
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
using namespace boost;
#include "writer.h"
#include "format.h"
#include "jsonconfig.h"
#include "log.h"

namespace Pleg {

namespace Files {

Files::~Files()
{
    Files::writers_vec_type writers(list());
    for(auto file : writers){
        file->getDevice().flush();
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
    FILESLOCK;
    virtualPath vpath(filename);
    if(!vpath.isValid()) {
        Pleg::Log() << "Attempted to access illegal path.";
        return;
    }
    Pleg::FileWriter *file(getFileWriter(vpath));
    if(!file){
        file = new Pleg::FileWriter(vpath);
        what.insert({vpath,file});
    }
    file->format.pattern.push_back(rel);//TODO add to record range
    std::pair<const Relevance,Pleg::Buffers::Acceptor*> arg(std::make_pair(rel,file));
    Pleg::Cache(CPS_call_void(Buffers::registerRelevance,arg));
}

void Files::remove(string const& filename)
{
    FILESLOCK;
    virtualPath vpath(filename);
    if(!vpath.isValid()) {
        Log() << "Attempted to access illegal path.";
        return;
    }
    Pleg::FileWriter *file(getFileWriter(vpath));
    if(!file){
        return;
    }
    Pleg::Buffers::Acceptor *acceptor(dynamic_cast<Pleg::Buffers::Acceptor*>(file));
    Pleg::Cache(CPS_call_void(Pleg::Buffers::unregisterAcceptor,acceptor));
}

Pleg::FileWriter *Files::getFileWriter(string const& filename)
{
    what_type::iterator it(what.find(filename));
    return it==what.end()?nullptr:(*it).second;
}

Files files;

virtualPath::virtualPath(string path):m_path(rootPath().string()+filesystem::path::preferred_separator+path)
{
    auto root(rootPath());
    if(!filesystem::exists(root))
        filesystem::create_directory(root);
}

filesystem::path virtualPath::absolutePath()
{
    return filesystem::absolute(m_path,filesystem::initial_path());
}

string virtualPath::relativePath()
{
    return isValid()?absolutePath().string().substr(rootPath().string().length()):m_path.string();
}

bool virtualPath::isValid()
{
    return algorithm::starts_with(absolutePath().string(),rootPath().string());
}

bool virtualPath::exists()
{
    return filesystem::exists(m_path);
}

virtualPath &virtualPath::operator +=(string path)
{
    m_path.append(path.c_str());
    return *this;
}

filesystem::path virtualPath::rootPath()
{
    Config::JsonConfig config(Config::load(Config::files_config_file));
    return filesystem::absolute(filesystem::path(config.at("/files_root").get_string()),filesystem::initial_path());
}

//

void getStatus(json::value *status)
{
    json::value array(json::empty_array);
    for(Files::what_type::value_type &pair : files.what){
        array.get_array().push_back(pair.first);
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
        std::ofstream stat(file.path().string().c_str());
        if(!stat.is_open())
            continue;
        json::value node(json::empty_object);
        string nodepath(filesystem::absolute(file.path(),filesystem::initial_path()).string());
        if(file.status().type() == directory_file){
            node.get_object().insert({"type","directory"});
            json::value children(json::empty_array);
            string error = treeAt(&children,nodepath);
            if(error.length()){
                tree->get_array().clear();
                tree->get_array().push_back(error);
                continue;
            }
            node.get_object().insert({"children",children});
        }else if(file.status().type() == regular_file){
            node.get_object().insert({"type","file"});
            node.get_object().insert({"size",filesystem::file_size(file.path())});
        }else{
            continue;
        }
        node.get_object().insert({"path",file.path().string()});
        node.get_object().insert({"name",file.path().filename().string()});
        tree->get_array().push_back(node);
    }
    return "";
}

} // namespace Files

} // namespace Pleg
