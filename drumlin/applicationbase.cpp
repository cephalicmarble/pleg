#include <tao/json.hpp>
using namespace tao;
#include "applicationbase.h"

namespace drumlin {

void ApplicationBase::getStatus(json::value *status)const
{
    lock_guard<mutex> l(const_cast<mutex&>(m_critical_section));
    json::value array(json::empty_array);
    for(threads_reg_type::value_type const& thread : *threads){
        json::value obj(json::empty_object);
        Thread *_thread(thread.second->getThread());
        if(!_thread->isStarted() || _thread->isTerminated())
            continue;
        thread.second->writeToObject(&obj);//report the thread
        array.get_array().push_back(obj);
        thread.second->getStatus(status);//report sub-system
    }
    status->get_object().insert({"threads",array});
}

} // namespace drumlin
