#ifndef OBJECT_H
#define OBJECT_H

#include <iostream>
#include <list>
#include <map>
#include <boost/any.hpp>
#include <boost/type_index.hpp>
#include <boost/functional.hpp>
#include <boost/preprocessor.hpp>
using namespace boost;
using namespace std;
#include "event.h"

namespace drumlin {

#define ARG(r,data,elem) ,elem
#define VA_ARG(r,data,elem) ,va_arg(ap,elem)

typedef map<boost::any,void*> any_pointer_map_type;
struct any_pointer_map_type_find {
    any_pointer_map_type_find(any_pointer_map_type &map):m_map(map){}
    void *operator()(boost::any & key)
    {
        for(auto & any_map_ptr : m_map) {
            if(key.type() == any_map_ptr.first.type())
                return any_map_ptr.second;
        }
        return nullptr;
    }
private:
    any_pointer_map_type m_map;
};

template <typename... Args>
struct namedMethodMetaObject {

};

class Object;

class Object
{
public:
    Object(Object *parent = nullptr):m_parent(parent){}
    virtual bool event(Event *){ return false; }
private:
    Object *m_parent;
};

} // namespace drumlin

#endif // OBJECT_H
