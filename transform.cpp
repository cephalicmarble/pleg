#include <pleg.h>
using namespace Pleg;
#include "transform.h"
#include <functional>
#include "response.h"

namespace Transforms {

/**
 * @brief Transform::error : ...the error slot which posts an application event.
 * @param string tring
 */
void Transform::error(const char*string)
{
    (new Event(Event::Type::TransformError,string))->punt();
}

void Passthrough::run(QObject *obj,Event *event)
{
    Debug() << "!!foobar";
    auto sub(Buffers::make_sub(getResponse()->getRequest()->getRelevanceRef(),(Acceptor*)this));
    auto relevant(Buffers::Cache(CPS_call_void(Buffers::registerRelevance,sub)));
    for(auto buffer : relevant){
        if(nullptr != buffer)
            accept(buffer);
    }
}

void Passthrough::accept(const Buffers::Buffer *buffer)
{
    char *str = const_cast<char*>(buffer->data<char>()),*p=str;
    do{
        *p = toupper(*p);
    }while(*(++p));
}

void Passthrough::flush(const Buffers::Buffer *buffer)
{

}

}
