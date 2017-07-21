#ifndef MUTEXCALL_H
#define MUTEXCALL_H

#include <mutex>
using namespace std;

namespace drumlin {

/**
 * @brief mutex_call_1 : template mutex call pointer to member function type
 */
template <class _Class,class _Return,class _Param>
struct mutex_call_1
{
    typedef _Class Class;
    typedef _Return Return;
    typedef _Param Param;
    typedef _Return (_Class::*func_type)(_Param);
    const func_type func;
    /**
     * @brief mutex_call_1::operator () : template function call operator
     * @param klass
     * @param param
     * @return
     */
    _Return operator()(_Class *klass,_Param param)
    {
        return (klass->*func)(param);
    }
    mutex_call_1(const func_type &_func):func(_func){}
};

/**
 * @brief Continuer : subclass this for use with mutex_call_1
 */
template <class Call>
class Continuer
{
    std::recursive_mutex mutex;
public:
    typedef typename Call::Return Param;
    /**
     * @brief Continuer : sets the QMutex::Recursive flag
     */
    Continuer(){}
    /**
     * @brief ~Continuer : necessary
     */
    virtual ~Continuer(){}
    /**
     * @brief Continuer::accept : template accept function
     * @param param template
     */
    virtual void accept(Call&,Param param)=0;
    /**
     * @brief lock : lock the mutex
     */
    void lock(){ mutex.lock(); }
    /**
     * @brief tryLock : tryLock the mutex
     * @return
     */
    bool tryLock() { return mutex.try_lock(); }
    /**
     * @brief unlock : unlock the mutex
     */
    void unlock(){ mutex.unlock(); }
};

/**
 * @brief CPS : template class for self-locking continuation passing style
 */
template <class Call,class Continue>
struct CPS {
    typedef mutex_call_1<typename Call::Class,typename Call::Return,typename Call::Param> func_type;
    Continue *continuer;
    typedef typename Call::Return Return;
    typename Call::Param param;
    typename Call::Return ret;
    func_type func;
    /**
     * @brief CPS::operator () : template
     * @param klass template
     * @return template
     */
    typename Call::Return operator()(typename Call::Class *klass)
    {
        ret = func(klass,param);
        if(continuer){
            while(!continuer->tryLock())
                boost::this_thread::yield();
            continuer->accept(func,ret);
            continuer->unlock();
        }
        return ret;
    }
    /**
     * @brief CPS::CPS : used by CPS_call and CPS_call_void
     * @param _continuer template
     * @param _func template
     * @param _param template
     */
    CPS(Continue *_continuer,func_type _func,typename Call::Param _param):continuer(_continuer),param(_param),func(_func){}
};

/**
 * @brief CPS_void : template class for thread-safe calls
 */
template <class Call,int = 0>
struct CPS_void {
    typedef mutex_call_1<typename Call::Class,typename Call::Return,typename Call::Param> func_type;
    typedef typename Call::Return Return;
    typename Call::Param param;
    typename Call::Return ret;
    func_type func;
    /**
     * @brief CPS_void::operator () : template function call operator for CPS_void
     * @param klass template
     * @return template
     */
    typename Call::Return operator()(typename Call::Class *klass)
    {
        ret = func(klass,param);
        return ret;
    }
    /**
     * @brief CPS_void::CPS_void : template constructor
     * @param _func template
     * @param _param template
     */
    CPS_void(func_type _func,typename Call::Param _param):param(_param),func(_func){}
};

/**
 * @brief CPS_call : template helper function to create call structures for thread-safe calls into objects
 */
template <class Class,class Return,class Param,class Continue>
CPS<mutex_call_1<Class,Return,Param>,Continue>
CPS_call(Continue *_continuer,mutex_call_1<Class,Return,Param> _func,Param _param)
{
    return CPS<mutex_call_1<Class,Return,Param>,Continue>(_continuer,_func,_param);
}
/**
 * @brief CPS_call_void : template helper function to create call structures for thread-safe calls into objects
 */
template <class Class,class Return,class Param>
CPS_void<mutex_call_1<Class,Return,Param>,0>
CPS_call_void(mutex_call_1<Class,Return,Param> _func,Param _param)
{
    return CPS_void<mutex_call_1<Class,Return,Param>>(_func,_param);
}

} // namespace drumlin

#endif // MUTEXCALL_H
