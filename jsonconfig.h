#ifndef JSONCONFIG_H
#define JSONCONFIG_H

#include "object.h"
using namespace drumlin;
#include <sstream>
#include <iostream>
#include <list>
#include <iostream>
#include <fstream>
using namespace std;
#include "tao_forward.h"
using namespace tao;

//#include <QBluetoothDeviceInfo>
//class BluetoothLEDevice;
//class LowEnergySource;
class Socket;

namespace Config {

/**
 * @brief The JsonConfig class
 */
class JsonConfig :
    public Object
{
private:
    json::value *json = nullptr;
    bool temporaryFlag;
public:
    static list<json::value> jsons;
    JsonConfig();
    JsonConfig(string const& path,bool temporaryFlag = true);
    JsonConfig(const JsonConfig &rhs);
    virtual ~JsonConfig();

    void clearJson();
public:
//    void writeDeviceSource(const QBluetoothDeviceInfo &info,LowEnergySource *_source);
    json::value *getDevice(const string &mac);

    void setDefaultValue(json::value *parent,const json::object_initializer &&);
    void setKey(const json::object_initializer && l);
    json::value *getKey(string const& key);
    json::value operator[](string const& key)const;
    json::value &at(string const& key)const;
    json::value *object();

//    json::value from(BluetoothLEDevice*);
    void from(string const& _json);
    void load(string const& path);
    void load(istream &device);
    void save(Socket *device);
    void save(ostream &device);
    void save(string const& path);
signals:
    void beforeSave(json::value *json);
    void afterLoad(json::value *json);

public:
    friend ostream &operator<<(ostream &stream,const JsonConfig &rel);
};

#define JSON_OBJECT_PROP(parent,name) (*parent.find(name)).toObject()

#define JSON_PROPERTY(parent,name) (*parent.find(name))

extern JsonConfig config;
extern string devices_config_file;
extern string gstreamer_config_file;
extern string files_config_file;

json::value *object(json::value *obj = 0);
json::value *array(json::value *array = 0);
size_t length(json::value *value);

}

#endif // JSONCONFIG_H
