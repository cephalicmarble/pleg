#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <memory>
using namespace std;
#include "thread.h"
#include "event.h"
#include "request.h"
#include "writer.h"
#include "buffer.h"
using namespace drumlin;

namespace Pleg {

class Writer;
class Response;

}

using namespace Pleg;

namespace Transforms {

/**
 * @brief The Transform class : abstract base class of processing elements
 */
class Transform :
    public ThreadWorker,
    public Buffers::Acceptor
{
    Response *response;
    std::unique_ptr<Writer> writer;
public:
    Transform(Thread *_thread,Response *parent) : ThreadWorker(_thread),Buffers::Acceptor() {
        response = parent;
    }
    void setWriter(Writer *_writer){ writer.reset(_writer); }
    Response *getResponse(){ return response; }

    /**
     * @brief writeToStream : for server status
     * @param stream std::ostream&
     */
    virtual void writeToStream(std::ostream &stream)const=0;
    virtual void getStatus(json::value *status)const{}
    virtual void report(json::value *obj,ReportType type)const{}

    virtual void error(const char*);

    friend class Post;
};

class Passthrough :
    public Transform
{
public:
    Passthrough(Thread *_thread,Response *parent = 0):Transform(_thread,parent){
        type = transform;
    }
    virtual void accept(const Buffers::Buffer *buffer);
    virtual void flush(const Buffers::Buffer *buffer);
public slots:
    virtual void run(QObject *obj,Event *event);
    virtual void writeToStream(std::ostream &stream)const{
        stream << "OK";
    }
    virtual void shutdown(){
        Debug() << "Passthrough shutdown";
    }
};

}

#endif // TRANSFORM_H
