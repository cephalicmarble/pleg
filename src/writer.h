#ifndef WRITER_H
#define WRITER_H

#include "drumlin/tao_forward.h"
using namespace tao;
#include <fstream>
#include <map>
#include <fstream>
using namespace std;
#include <boost/date_time/posix_time/posix_time.hpp>
using namespace boost;
#include "drumlin/object.h"
using namespace drumlin;
#include "relevance.h"
#include "buffer.h"
#include "format.h"

//namespace drumlin {
//    class Socket;
//}

namespace Pleg {

/*
 * Forward declarations
 */

class Response;

/**
 * @brief The Writer class
 */
class Writer :
    public Object
{
public:
    explicit Writer(Object *parent = 0):Object(parent){}
    /**
     * @brief write : pure virtual function to write a buffer.
     * @param buffer Buffers::Buffer*
     */
    virtual void write(const Buffers::Buffer *buffer)=0;
    virtual void write(byte_array const& bytes)=0;

    json::value *getJsonObject(const Buffers::Buffer *buffer);
    void writeJson(const Buffers::Buffer *buffer);
    void writeJson(const Buffers::buffer_vec_type buffers);
    void writeMetaJson(const string &source);
    void writeBuffer(const Buffers::Buffer*);
};

/**
 * @brief The ResponseWriter class writes over HTTP
 */
class ResponseWriter :
    public Writer
{
public:
    /**
     * @brief getResponse
     * @return Response*
     */
    Response *getResponse(){return (Response*)parent(); }
    explicit ResponseWriter(Response *parent):Writer((Object*)parent){}
public:
    virtual void write(const Buffers::Buffer *buffer);
    virtual void write(byte_array const& bytes);
};

class FileWriter :
    public Writer,
    public Buffers::Acceptor
{
    explicit FileWriter(string const& filename);
public:
    Pleg::Files::Format format;
    string getFilePath()const{return filePath;}
    void write(const Buffers::Buffer *buffer);
    void write(byte_array const& bytes);
    void getStatus(json::value *status);

    void accept(const Buffers::Buffer *buffer);
    void flush(const Buffers::Buffer *buffer);

    friend class Files::Files;
    friend void Files::getStatus(json::value *status);
    /**
     * @brief setDevice
     * @param _device ofstream
     */
    void setDevice(std::ofstream &&_device){
        device = std::move(_device);
    }
    /**
     * @brief getDevice
     * @return ofstream
     */
    std::ofstream &getDevice(){
        return device;
    }
private:
    string filePath;
    std::ofstream device;
    posix_time::ptime current;
};

///**
// * @brief The SocketWriter class transmits over UDP
// */
//class SocketWriter :
//    public Writer
//{
//public:
//    explicit SocketWriter(QObject *parent = 0):Writer(parent){}
//    /**
//     * @brief setSocket
//     * @param _Socket ofstream
//     */
//    void setSocket(ofstream &&_Socket){
//        m_socket = std::move(_Socket);
//    }
//    /**
//     * @brief getSocket
//     * @return ofstream
//     */
//    ofstream &getSocket(){
//        return m_socket;
//    }
//private:
//    Socket *m_socket;
//};

} // namespace Pleg

#endif // WRITER_H
