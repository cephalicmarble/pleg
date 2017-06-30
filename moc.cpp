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

typedef map<Object*,string> connection_map_type;
typedef map<string,void*> method_map_type;

struct metaObject {
    method_map_type the_slots;
    method_map_type the_signals;
    method_map_type signalMap;
};

class Object
{
    struct objectFactory {
        list<void (*)(Object*,metaObject*)> initializers;
        metaObject *create(Object *that)
        {
            metaObject *what = new metaObject();
            for(auto &init : initializers) {
                init(that,what);
            }
        }
    };
public:
    static objectFactory staticFactory;
    Object(Object *m_parent = nullptr):m_metaObject(staticFactory.create(this)){}

    metaObject *getMetaObject(){ return m_metaObject; }
    void *get_method_object(string Identifier)
    {
        metaObject *meta(getMetaObject());
        if(meta->the_slots.end()==meta->the_slots.find(Identifier)){
            if(meta->the_signals.end()==meta->the_signals.find(Identifier)){
                return nullptr;
            }
            return meta->the_signals.at(Identifier);
        }
        return meta->the_slots.at(Identifier);
    }

    bool connect(Object *signaller,string signal,Object *slotter,string slot)
    {
        if(!signaller->get_method_object(signal))
            return false;
        if(!slotter->get_method_object(slot))
            return false;
        connection_map_type *slotList(getMetaObject()->signalMap.at(signal));
        slotList->insert(slotter,slot);
    }
private:
    Object *m_parent;
    metaObject *m_metaObject;
};

//typedef function<void (Classname*,void* BOOST_PP_LIST_FOR_EACH(ARG,,BOOST_PP_TUPLE_TO_LIST(BOOST_PP_TUPLE_SIZE(ArgList),ArgList)))> signal_signature_##Identifier; \

#define SIGNAL(Identifier) Identifier##_namedMethod

#define SIGNAL(Classname,Identifier,ArgList) \
    connection_map_type slotList_##Identifier; \
public: \
    struct Identifier##_namedMethod { \
        static void init(Object *that,metaObject *what) \
        { \
            Classname *klass = ((Classname*)that); \
            what->the_signals.insert({#Identifier,new Identifier##_namedMethod}); \
        } \
        Identifier##_namedMethod() \
        { \
            Object::staticFactory.initializers.push_back({&Identifier##_namedMethod::init}); \
        } \
        void operator()(Classname *klass,...){ \
            va_list ap; \
            for(auto & pair : slotList_##Identifier) { \
                va_start(ap,klass); \
                auto &slot = pair.first->get_slot_##BOOST_PP_TUPLE_SIZE(ArgList)(pair.second); \
                slot(klass BOOST_PP_LIST_FOR_EACH(VA_ARG,,BOOST_PP_TUPLE_TO_LIST(BOOST_PP_TUPLE_SIZE(ArgList),ArgList))); \
                va_end(ap); \
            } \
        } \
    }; \
    static Identifier##_namedMethod Identifier##_namedMethod_init; \
    void Identifier(Classname *klass,...) { \

    }

#define SLOT(Classname,Identifier,ArgList) \
    typedef function<void (Classname* BOOST_PP_LIST_FOR_EACH(ARG,,BOOST_PP_TUPLE_TO_LIST(BOOST_PP_TUPLE_SIZE(ArgList),ArgList)))> slot_signature_##Identifier; \
    const slot_signature_##Identifier Identifier = nullptr; \
    struct Identifier##_namedMethod { \
        static void init(Object *that,metaObject *what) \
        { \
            Classname *klass = ((Classname*)that); \
            what->slotMap.insert({#Identifier,new Identifier##_namedMethod}); \
        } \
        Identifier##_namedMethod() \
        { \
            Object::staticFactory.initializers.push_back({&Identifier##_namedMethod::init}); \
        } \
        template <class T,typename... Args> \
        void operator()(Classname *klass,Args... args){ \
            klass->Identifier##_impl(args...); \
        } \
    }; \
    static Identifier##_namedMethod Identifier##_namedMethod_init; \
    void Identifier##_impl ArgList;

class Class : public Object
{
public:
    Class():Object(0){
        connect(this,SIGNAL(signal),this,SLOT(slot));
    }
    void signal(int,char);
    void slot(int,char);
    SIGNAL(Class,signal,(int,char))
    SLOT(Class,slot,(int,char))
    void foo() {
        signal(this,1,'c');
    }
};

void Class::slot_impl(int,char)
{
    cout << "twat. Keith is a twat";
}

int main()
{
    Class c;
    c.foo();
}

} // namespace drumlin

#endif // OBJECT_H
