#include "registry.h"

#include "thread.h"
#include "source.h"

template <>
void Registry<WorkObject>::remove(const std::string &str,bool noDelete)
{
    QMutexLocker ml(&mutex);
    typename map_type::iterator it(map->find(str));
    if(it!=map->end()){
        (*it).second->stop();
        if(!noDelete)
            delete (*it).second;
        map->erase(it);
    }
}

template <class Type>
void QSRegistry<Type>::remove(const std::string &str,bool noDelete)
{
    QMutexLocker ml(&mutex);
    typename map_type::iterator it(map->find(str));
    if(it!=map->end()){
        if(!noDelete)
            delete (*it).second;
        map->erase(it);
    }
}

template class QSRegistry<Sources::QSSource>;
template class QSRegistry<QSThreadWorker>;
