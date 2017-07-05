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
    Transform(drumlin::Thread *_thread,Response *parent) : ThreadWorker(ThreadType_transform,_thread),Buffers::Acceptor() {
        response = parent;
    }
    void setWriter(Writer *_writer){ writer.reset(_writer); }
    Response *getResponse(){ return response; }

    /**
     * @brief writeToStream : for server status
     * @param stream std::ostream&
     */
    virtual void writeToStream(std::ostream &stream)const=0;
    virtual void getStatus(json::value *)const{}
    virtual void report(json::value *,ReportType)const{}

    virtual void error(const char*);

    friend class Post;
};

class Passthrough :
    public Transform
{
public:
    Passthrough(drumlin::Thread *_thread,Response *parent = 0):Transform(_thread,parent){}
    virtual void accept(const Buffers::Buffer *buffer);
    virtual void flush(const Buffers::Buffer *buffer);
    virtual void run(Object *obj,Event *event);
    virtual void writeToStream(std::ostream &stream)const{
        stream << "OK";
    }
    virtual void shutdown(){
        Debug() << "Passthrough shutdown";
    }
};

} // namespace Transforms

} // namespace Pleg

#endif // TRANSFORM_H
