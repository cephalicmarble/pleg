#include <tao/json.hpp>
using namespace tao;
#include <pleg.h>
using namespace Pleg;
#include <iostream>
#include <fstream>
using namespace std;
#include "byte_array.h"
using namespace drumlin;
#include "jsonconfig.h"
#include <boost/filesystem.hpp>
#include <boost/uuid/uuid.hpp>
//#include "bluetooth.h"
//#include "lowenergy.h"
//#include "device.h"
#include "socket.h"

namespace Config {

json_map_clearer klaar;

json_map_type JsonConfig::s_jsons;

void reload()
{
    for(json_map_type::value_type &config : JsonConfig::s_jsons){
        delete config.second;
        config.second = load(config.first).object();
    }
}

JsonConfig load(string path)
{
    return JsonConfig(path);
}

JsonConfig::JsonConfig() : Object(0),temporaryFlag(true)
{
    json = fromJson("{}");
}

/**
 * @brief JsonConfig::JsonConfig
 */
JsonConfig::JsonConfig(std::string const& path) : Object(0),temporaryFlag(false)
{
    json = fromFile(path);
}

/**
 * @brief JsonConfig::JsonConfig : copy constructor
 * @param rhs JsonConfig&
 */
JsonConfig::JsonConfig(const JsonConfig &rhs) :
    Object(0),json(rhs.json),temporaryFlag(rhs.temporaryFlag)
{
}

/**
 * @brief JsonConfig::~JsonConfig
 */
JsonConfig::~JsonConfig()
{
    if(temporaryFlag){
        delete json;
    }
}

/**
 * @brief JsonConfig::object : return the config hash
 * @return QJsonObject
 */
json::value *JsonConfig::object()
{
    return json;
}

/**
 * @brief JsonConfig::getKey : return a key from the config
 * @param key tring
 * @return QJsonValue
 */
json::value *JsonConfig::getKey(std::string const& key)
{
    return json->find(key);
}

/**
 * @brief JsonConfig::operator []
 * @param key tring
 * @return json::value*
 */
json::value JsonConfig::operator[](std::string const& key)const
{
    json::pointer ptr(key.c_str());
    return json->at(ptr);
}

/**
 * @brief JsonConfig::operator []
 * @param key tring
 * @return json::value*
 */
json::value &JsonConfig::at(std::string const& key)const
{
    json::pointer ptr(key.c_str());
    return json->at(ptr);
}

/**
 * @brief JsonConfig::setKey : set a key in the config
 * @param key tring
 * @param value QJsonValue
 */
void JsonConfig::setKey(const json::object_initializer && l)
{
    json->get_object().insert(std::move(l));
}

//json::value JsonConfig::from(BluetoothLEDevice *that)
//{
//    return at(std::string("/devices/")+that->getDeviceInfo().address().toString().toStdString());
//}

json::value *JsonConfig::fromJson(std::string const& _json)
{
    json = new json::value(json::from_string(_json));
    return json;
}

/**
 * @brief JsonConfig::load : load from file
 * @param path tring
 */
json::value *JsonConfig::fromFile(std::string const& path)
{
    json_map_type::iterator it(s_jsons.find(path));
    if(s_jsons.end()!=it){
        json = it->second;
        return json;
    }
    filesystem::path p(path);
    if(!filesystem::exists(p))
        return nullptr;
    ifstream strm(path.c_str());
    stringstream ss;
    ss << strm.rdbuf();
    json = new json::value(json::from_string(ss.str()));
    s_jsons.insert({path,json});
    return json;
}

/**
 * @brief JsonConfig::save : save to device
 * @param device ostream*
 */
void JsonConfig::save(ostream &device)
{
    json::to_stream(device,*json,2);
}

/**
 * @brief JsonConfig::save : save to request
 * @param request Request*
 */
void JsonConfig::save(Pleg::Request *request)
{
    std::string str(json::to_string(*json));
    request->getSocketRef().write(str);
}

/**
 * @brief JsonConfig::save : save to file
 * @param path tring
 */
void JsonConfig::save(std::string const& path)
{
    filesystem::path p(path);
    if(!filesystem::exists(p))
        return;
    ofstream strm(path.c_str());
    if(!strm.is_open()){
        Debug() << "could not write config '" << path.c_str() << "'";
        return;
    }
    save(strm);
}

/**
 * @brief JsonConfig::setDefaultValue : convenience function
 * @param parent QJsonObject
 * @param key tring
 * @param _default QJsonValue
 */
void JsonConfig::setDefaultValue(json::value *parent, const json::object_initializer && l)
{
    for(auto _l : l){
        if(!parent->find(_l.first))
            parent->get_object().insert(_l);
    }
}

///**
// * @brief JsonConfig::writeDeviceSource : write a bluetooth le device source to the config
// * @param info QBluetoothDeviceInfo
// * @param _source LowEnergySource*
// */
//void JsonConfig::writeDeviceSource(const QBluetoothDeviceInfo &info,LowEnergySource *_source)
//{
//    bool empty_device = false;
//    json::value *devices = Config::object(&(*json)["devices"]);
//    json::value *device = Config::object(&devices->get_object()[info.address().toString().toStdString()]);
//    if(!Config::length(device))
//        empty_device = true;
//    json::value *services = Config::object(&(*device)["services"]);
//    std::string service_uuid(_source->getService()->serviceUuid().toString().toStdString()),
//                service_name(_source->getService()->serviceName().toStdString()),
//                name;
//    json::value *service(Config::object(&(*services)[service_uuid]));
//    json::value *characteristics = Config::array(&(*service)["characteristics"]);
//    std::for_each(_source->copyCharacteristicsRef().begin(),_source->copyCharacteristicsRef().end(),
//    [&characteristics,_source,&name](const LowEnergySource::copy_chars_type::value_type &_char){
//        if(!_char.isValid())
//            return;
//        if(std::find_if(characteristics->get_array().begin(),characteristics->get_array().end(),
//        [_char](auto characteristic){
//            return (characteristic.is_object()
//                 && characteristic.get_object().find("desc") != characteristic.get_object().end()
//                 && QBluetoothUuid(tring(characteristic.get_object().find("desc")->second.get_string().c_str())) == _char.uuid());
//        }) == characteristics->get_array().end()) {
//            std::string n(_char.name().toStdString());
//            name.clear();
//            for(quint16 i=0;i<n.length();i++){
//                if(isupper(n[i])){
//                    name += n[i];
//                }
//            }
//            json::value char_{
//                {"name",_char.name().toStdString().c_str()},
//                {"desc",_char.uuid().toString().toStdString().c_str()},
//            };
//            characteristics->get_array().push_back(char_);
//        }
//    });
//    service->get_object().insert({
//        {"uuid",service_uuid.c_str()},
//        {"name",service_name.c_str()},
//        {"source",name.c_str()},
//        {"characteristics",*characteristics},
//    });
//    services->get_object().insert(std::make_pair(service_uuid.c_str(),*service));

//    if(empty_device){
//        device->get_object().insert({
//            {"name",info.name().toStdString().c_str()},
//            {"class",(quint32)info.serviceClasses()},
//            {"services",*services},
//            {"addr",info.address().toString().toStdString().c_str()},
//        });
//        devices->get_object().insert({
//            {info.address().toString().toStdString().c_str(),*device}
//        });
//    }else{
//        device->get_object().insert({"services",*services});
//        devices->get_object().insert({info.address().toString().toStdString().c_str(),*device});
//    }
//    json->get_object().insert({"devices",*devices});
//}

///**
// * @brief JsonConfig::getDevice : get device from the config
// * @param mac tring
// * @return JsonConfig::devices_return_type
// */
//json::value *JsonConfig::getDevice(const tring &mac)
//{
//    json::value *devices = Config::object();
//    json::value _devices((*this)["/devices"]);
//    if(!_devices.is_object())
//        return devices;
//    for(auto dev_conf : _devices.get_object()){
//        json::value *device(const_cast<json::value*>(&dev_conf.second));
//        std::string addr = dev_conf.first;
//        if(!mac.compare("all") || !mac.compare(addr.c_str())){
//            const json::value *_class(&device->get_object()["class"]);
//            QBluetoothDeviceInfo info(QBluetoothAddress(mac),device->get_object()["name"].get_string().c_str(),_class->empty()?0:_class->get_unsigned());
//            json::value dev = *device;
//            devices->get_object().insert({addr,dev});
//        }
//    }
//    return devices;
//}

std::string devices_config_file;
std::string gstreamer_config_file;
std::string files_config_file;

json::value *object(json::value *obj)
{
    if(!obj) obj = new json::value;
    if(!*obj) obj->prepare_object();
    return obj;
}
json::value *array(json::value *array)
{
    if(!array) array = new json::value;
    if(!*array) array->prepare_array();
    return array;
}
size_t length(json::value *value)
{
    if(value->is_array()){
        return std::distance(value->get_array().begin(),value->get_array().end());
    }else if(value->is_object()){
        return std::distance(value->get_object().begin(),value->get_object().end());
    }else{
        return value->empty()?0:1;
    }
}

/**
 * @brief operator << : stream operator
 * @param stream std::ostream
 * @param rel JsonConfig
 * @return std::ostream
 */
logger &operator<<(logger &stream,const JsonConfig &rel)
{
    json::to_stream(stream,*rel.json);
    return stream;
}

json_map_clearer::~json_map_clearer()
{
    for(json_map_type::value_type & pair : JsonConfig::s_jsons){
        delete pair.second;
    }
}

} // namespace Config
