#include <pleg.h>
using namespace Pleg;
#include <tao/json.hpp>
using namespace tao;
#include <functional>
#include <fstream>
using namespace std;
#include <boost/uuid/uuid.hpp>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
using namespace boost;
#include "writer.h"
#include "buffer.h"
#include "response.h"
#include "request.h"
#include "socket.h"
//#include "lowenergy.h"
#include "files.h"

namespace Pleg {

/**
 * @brief Writer::writeBuffer : call virtual void write(buffer);
 * @param buffer Buffers::Buffer*
 */
void Writer::writeBuffer(const Buffers::Buffer *buffer)
{
    write(buffer);
}

void Writer::writeJson(const Buffers::buffer_vec_type buffers)
{
    std::vector<json::value*> vec;
    {
        json::value array(json::empty_array),*value;
        for(auto buffer : buffers){
            value = getJsonObject(buffer);
            vec.push_back(value);
            array.get_array().push_back(*value);
        }
        write(byte_array(json::to_string(array)));
    }
    for(auto value : vec){
        delete value;
    }
}

/**
 * @brief ResponseWriter::writeJson : resolve sources we know about
 * (Visitor pattern) and let them write to our object.
 * @param buffer Buffers::Buffer*
 */
void Writer::writeJson(const Buffers::Buffer *buffer)
{
    json::value *object(getJsonObject(buffer));
    string str(json::to_string(*object));
    write(byte_array(str));
    delete object;
}

json::value *Writer::getJsonObject(const Buffers::Buffer *buffer)
{
    uuids::uuid uuid(buffer->getRelevanceRef().getUuid());
    json::value *object(Config::object());
    stringstream ss;
    ss << uuid;
    object->get_object().insert({
        { "uuid", ss.str() },
        { "timestamp", posix_time::to_simple_string(buffer->getTimestampRef()) }
    });
    do{
        Sources::Source *source(buffer->getRelevanceRef().getSource());
        object->insert({ {"name",source->getName()} });
//        Sources::HeartRateSource *hrs(dynamic_cast<Sources::HeartRateSource*>(source));
//        if(uuid==Sources::HeartRateSource::UUID && hrs){
//            const Sources::HeartRateData *data(buffer->data<Sources::HeartRateData>());
//            if(data->hasHeartRate()){
//                object->get_object().insert({"heart_rate",(unsigned int)data->getHeartRate()});
//            }
//            if(data->hasSensorContact()){
//                object->get_object().insert({"sensor_contact",(bool)data->isInContact()});
//            }
//            if(data->hasEnergyExpended()){
//                object->get_object().insert({"energy_expended",(unsigned int)data->getEnergyExpended()});
//            }
//            if(data->hasRRIntervals()){
//                quint16 n;
//                quint16 *rr(const_cast<Sources::HeartRateData*>(data)->getRRIntervals(&n));
//                json::value array(json::empty_array);
//                for(int i=0;i<n;i++)
//                    array.get_array().push_back(rr[i]);
//                object->get_object().insert({"rr_intervals",array});
//            }
//            break;
//        }
//        Sources::BatteryLevelSource *bls(dynamic_cast<Sources::BatteryLevelSource*>(source));
//        if(uuid==Sources::BatteryLevelSource::UUID && bls){
//            object->get_object().insert({"value",(unsigned int)buffer->data<Sources::BatteryData>()->getBatteryLevel()});
//            break;
//        }
        Sources::MockSource *mock(dynamic_cast<Sources::MockSource*>(source));
        if(uuid==uuids::uuid() && mock){
            object->insert({ {"mock",*buffer->data<guint64>()} });
            break;
        }
        Sources::GStreamerOffsetSource *offset(dynamic_cast<Sources::GStreamerOffsetSource*>(source));
        if(offset){
            object->insert({ {"offset",*buffer->data<guint64>()} });
            break;
        }
    }while(false);
    return object;
}

/**
 * @brief MockSource::meta : return charting metadata
 * @param tring source
 */
void Writer::writeMetaJson(const string &_source)
{
    Sources::Source *source(Sources::sources.fromString<Sources::Source>(_source));
    json::value object{{
        { "source", _source.c_str() }
    }};
    do{
//        Sources::HeartRateSource *hrs(dynamic_cast<Sources::HeartRateSource*>(source));
//        if(nullptr != hrs){
//            object.get_object().insert({ "charts", json::value::array({ { { "heart_rate", "BPM" } }, { { "rr_intervals", "RR" } } }) });
//            break;
//        }
//        Sources::BatteryLevelSource *bls(dynamic_cast<Sources::BatteryLevelSource*>(source));
//        if(nullptr != bls){
//            object.get_object().insert({ "charts", json::value::array({ { { "value", "Battery" } } }) });
//            break;
//        }
        Sources::MockSource *mock(dynamic_cast<Sources::MockSource*>(source));
        if(nullptr != mock){
            object.get_object().insert({ "charts", json::value::array({ { { "mock", "mock" } } }) });
            break;
        }
    }while(false);
    string str(json::to_string(object));
    write(byte_array(str.c_str(),str.length()));
}

/**
 * @brief ResponseWriter::write : write the buffer out the response channel
 * @param buffer Buffers::Buffer*
 */
void ResponseWriter::write(const Buffers::Buffer *buffer)
{
    getResponse()->getRequest()->getSocketRef().write(byte_array(buffer->data<const char>(),buffer->length()));
}

void ResponseWriter::write(byte_array const& bytes)
{
    getResponse()->getRequest()->getSocketRef().write(bytes);
}

FileWriter::FileWriter(string const& filename)
    :Writer(0),Buffers::Acceptor(),filePath(filename),device(std::ofstream(filename))
{
}

void FileWriter::accept(const Buffers::Buffer *buffer)
{
//    qDebug() << buffer->getRelevanceRef();
    write(buffer);
}

void FileWriter::flush(const Buffers::Buffer *)
{
    device.flush();
}

void FileWriter::write(const Buffers::Buffer *buffer)
{
    current = buffer->getTimestampRef();
    format.recordBuffer(buffer);
    json::value *object(getJsonObject(buffer));
    string s(json::to_string(*object));
    device << s.c_str();
    delete object;
}

void FileWriter::write(byte_array const& bytes)
{
    device << bytes;
}

void FileWriter::getStatus(json::value *status)
{
    filesystem::path p(filePath);
    json::object_t &obj(status->get_object());
    obj.insert({"path",filePath});
    obj.insert({"size",filesystem::file_size(p)});
    obj.insert({"current",posix_time::to_iso_string(current)});
    json::value _format(json::empty_object);
    format.getStatus(&_format);
    obj.insert({"format",_format});
}

} // namespace Pleg
