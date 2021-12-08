#ifndef FILES_H
#define FILES_H

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
using namespace boost;
#include "drumlin/object.h"
#include "drumlin/jsonconfig.h"
#include "relevance.h"
#include "source.h"

namespace Pleg {

class FileWriter;

namespace Files {

/**
 * @brief The FileWriter class : handles a number of files
 */
class Files
{
public:
    typedef map<string,Pleg::FileWriter*> what_type;
    what_type what;
    typedef vector<Pleg::FileWriter*> writers_vec_type;
    Files(){}
    virtual ~Files();
public:
    writers_vec_type list()const;
    void add(Relevance const& rel,string const& filename);
    void remove(string const& filename);
    void addFromIndex(json::value *json);
    FileWriter *getFileWriter(string const& filename);
    friend void getStatus(json::value *status);
};

extern Files files;

class virtualPath
{
public:
    virtualPath(string path);
    boost::filesystem::path absolutePath();
    string relativePath();
    bool isValid();
    static boost::filesystem::path rootPath();
    bool exists();
    void create();
    virtualPath &operator +=(std::string path);
    operator boost::filesystem::path(){ return m_path; }
    operator string(){ return m_path.string(); }
private:
    boost::filesystem::path m_path;
};

extern void getStatus(json::value *status);
extern string treeAt(json::value *tree,string root);

} // namespace Files

} // namespace Pleg

#endif // FILES_H
