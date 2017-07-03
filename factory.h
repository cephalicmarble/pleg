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
        lock_guard l(&factory_mutex); \
        Thread *thread(new Thread(#Class)); \
        Class *obj(new Class(thread,parent)); \
        obj->connectSlots(); \
        Container.push_back(obj); \
        return obj; \
    }\
}
/*
 * define a factory function forward declaration
 */
#define defFactoryHeader(Factory,Class,Parent) \
namespace Factory { \
    Class *create##Class(Parent *parent); \
}
/*
 * define a removal function to delete an instance
 * and update the container.
 */
#define defRemove(Factory,Class,Container) \
namespace Factory { \
    void remove(Class *transform){ \
        lock_guard l(&factory_mutex); \
        Container.remove(transform); \
        delete transform; \
    }\
}
/*
 * define a removal function
 */
#define defRemoveHeader(Factory,Class) \
namespace Factory { \
    void remove(Class *transform); \
}

namespace Factory{
    using namespace Transforms;

    /**
     * @brief transforms_type : the type of the factory container
     */
    typedef std::list<Transforms::Transform*> transforms_type;

    defRemoveHeader(Transform,Passthrough)
    defFactoryHeader(Transform,Passthrough,Response)

    namespace Response {

        Response *create(Request *that);

    }
}

#endif // FACTORY_H
