#ifndef FACTORY_H
#define FACTORY_H

#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>
using namespace boost;
#include <list>
using namespace std;
#include "event.h"
#include "transform.h"
using namespace drumlin;

namespace Pleg {

class Response;
class Request;

}

using namespace Pleg;

/*
 * define a factory namespace that calls connectSlots
 * on the created object, and calls push_back on the
 * std::container that keeps a record of the instances.
 */
#define defFactory(Factory,Class,Parent,Container) \
namespace Factory { \
    Class *create##Class(Parent *parent){ \
        lock_guard<recursive_mutex> l(Pleg::factory_mutex); \
        Thread *thread(new Thread(#Class)); \
        Class *obj(new Class(thread,parent)); \
        Container.push_back(obj); \
        return obj; \
    }\
}
/*
 * define a factory function forward declaration
 */
#define defFactoryHeader(ns,Factory,Class,Parent) \
namespace Factory { \
    ns::Class *create##Class(Parent *parent); \
}
/*
 * define a removal function to delete an instance
 * and update the container.
 */
#define defRemove(Factory,Class,Container) \
namespace Factory { \
    void remove(Class *transform){ \
        lock_guard<recursive_mutex> l(Pleg::factory_mutex); \
        Container.remove(transform); \
        delete transform; \
    }\
}
/*
 * define a removal function
 */
#define defRemoveHeader(ns,Factory,Class) \
namespace Factory { \
    void remove(ns::Class *transform); \
}

namespace Pleg {
    using namespace Transforms;

    typedef std::list<Transforms::Transform*> transforms_type;

    extern recursive_mutex factory_mutex;

    extern transforms_type transforms;
}

defRemoveHeader(Pleg::Transforms,Transform,Passthrough)
defFactoryHeader(Pleg::Transforms,Transform,Passthrough,Response)

namespace Factory{
    namespace Response {
        Pleg::Response *create(Pleg::Request *that);
    }
}

#endif // FACTORY_H
