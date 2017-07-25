#ifndef JSONCONFIG_H
#define JSONCONFIG_H

#include <sstream>
#include <iostream>
#include <list>
#include <iostream>
#include <fstream>
using namespace std;
#include "tao_forward.h"
using namespace tao;
#include "object.h"
#include "socket.h"
#include "request.h"
using namespace drumlin;

//#include <QBluetoothDeviceInfo>
//class BluetoothLEDevice;
//class LowEnergySource;

namespace Config {

class JsonConfig;

extern void reload();
extern JsonConfig load(string path);

typedef map<string,json::value*> json_map_type;

/**
 * @brief The JsonConfig class
 */
class JsonConfig :
    public Object
{
public:
    JsonConfig(const JsonConfig &rhs);
    virtual ~JsonConfig();

//    void writeDeviceSource(const QBluetoothDeviceInfo &info,LowEnergySource *_source);
    json::value *getDevice(const string &mac);

    void setDefaultValue(json::value *parent,const json::object_initializer &&);
    void setKey(const json::object_initializer && l);
    json::value *getKey(string const& key);
    json::value operator[](string const& key)const;
    json::value &at(string const& key)const;
    json::value *object();

//    json::value from(BluetoothLEDevice*);
    void save(Pleg::Request *request);
    void save(ostream &device);
    void save(string const& path);
public:
    friend logger &operator<<(logger &stream,const JsonConfig &rel);
    friend void reload();
    friend JsonConfig load(string path);
    static json_map_type s_jsons;
private:
    JsonConfig();
    JsonConfig(string const& path);
    json::value *fromJson(string const& _json);
    json::value *fromFile(string const& path);
    json::value *json = nullptr;
    bool temporaryFlag;
};

#define JSON_OBJECT_PROP(parent,name) (*parent.find(name)).toObject()

#define JSON_PROPERTY(parent,name) (*parent.find(name))

extern string devices_config_file;
extern string gstreamer_config_file;
extern string files_config_file;

json::value *object(json::value *obj = 0);
json::value *array(json::value *array = 0);
size_t length(json::value *value);

extern logger &operator<<(logger &stream,const JsonConfig &rel);

class json_map_clearer
{
public:
    ~json_map_clearer();
};

extern json_map_clearer klaar;

}

#endif // JSONCONFIG_H
