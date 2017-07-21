#ifndef REGISTRY_H
#define REGISTRY_H

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
using namespace std;
#include <boost/utility/enable_if.hpp>
#include <tao_forward.h>
using namespace tao;
#include "metatypes.h"

namespace drumlin {

class WorkObject
{
    enum WorkReportType {
        Elapsed  = 1,
        Jobs     = 2,
        Memory   = 4,
        All = Elapsed|Jobs|Memory,
    };
public:
    typedef WorkReportType ReportType;
    virtual ~WorkObject(){}
    virtual void stop(){}
    virtual void report(json::value *obj,ReportType type)const=0;
};

#define REGISTRYLOCK std::lock_guard<std::recursive_mutex> l(const_cast<std::recursive_mutex&>(mutex));

template <class Type>
class Registry
{
    /**
     * @brief le_devices_type : map of low energy devices
     */
public:
    typedef std::map<string,Type*> map_type;
    typedef typename map_type::value_type value_type;
    typedef typename map_type::iterator iterator;
    typedef typename map_type::const_iterator const_iterator;
    typedef Type mapped_type;
protected:
    map_type map;
public:
    std::recursive_mutex mutex;
    Registry(){}
    ~Registry(){}

    typedef vector<string> names_type;
    /**
     * @brief begin
     * @return map_type::iterator
     */
    const_iterator begin()const{ return map.begin(); }
    /**
     * @brief end
     * @return map_type::iterator
     */
    const_iterator end()const{ return map.end(); }
    /**
     * @brief add : TBC from Server only, inserts a new string Source* mapping
     * @param str string
     * @param src Source*
     */
    void add(const string &str,Type *src)
    {
        REGISTRYLOCK
        map.insert({str,src});
    }

    /**
     * @brief remove : remove a string Source* mapping, delete the Source
     * @param str
     */
    void remove(const string &str,bool noDelete = false)
    {
        REGISTRYLOCK
        typename map_type::iterator it(map.find(str));
        if(it!=map.end()){
            if(!noDelete)
                delete (*it).second;
            map.erase(it);
        }
    }

    /**
     * @brief removeAll : remove a string Source* mapping, delete the Source
     * @param str
     */
    void removeAll(bool noDelete = false)
    {
        REGISTRYLOCK
        for(auto pair : map){
            WorkObject *wo(dynamic_cast<WorkObject*>(pair.second));
            if(!!wo) wo->stop();
            if(!noDelete)
                delete wo;
        }
        map.clear();
    }

    /**
     * @brief list : list the named registered entities
     * @return vector<string>
     */
    names_type list()
    {
        REGISTRYLOCK
        names_type list;
        for(auto pair : map){
            list.push_back(pair.first);
        }
        return list;
    }

    /**
     * @brief fromString : find the named source
     * @param name string
     * @return Source*
     */
    template <class T = Type>
    T *fromString(const string &name)const
    {
        REGISTRYLOCK
        typename map_type::iterator it(find_if(const_cast<map_type&>(map).begin(),const_cast<map_type&>(map).end(),[name](typename map_type::value_type const& pair){
            return name == pair.first;
        }));
        if(it==const_cast<map_type&>(map).end()){
            return nullptr;
        }else{
            return dynamic_cast<T*>((*it).second);
        }
    }
    /**
     * @brief fromString : find the named source
     * @param name string
     * @return Source*
     */
    template <class T = Type>
    T * fromString(const char *str)const
    {
        return dynamic_cast<T*>(fromString<T>(string(str)));
    }
};

} // namespace drumlin

#endif // REGISTRY_H
