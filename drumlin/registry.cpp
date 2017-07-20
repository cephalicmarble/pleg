#include "registry.h"

#include "thread.h"
#include "source.h"
#include <boost/thread/lock_guard.hpp>

namespace drumlin {

template <>
void Registry<WorkObject>::remove(const std::string &str,bool noDelete)
{
    lock_guard<recursive_mutex> l(mutex);
    typename map_type::iterator it(map->find(str));
    if(it!=map->end()){
        (*it).second->stop();
        if(!noDelete)
            delete (*it).second;
        map->erase(it);
    }
}

template class Registry<drumlin::ThreadWorker>;

} // namespace drumlin