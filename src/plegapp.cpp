#include "plegapp.h"

#include <boost/regex.hpp>
using namespace boost;
#include "drumlin/thread.h"
#include "drumlin/event.h"
#include "drumlin/application.h"
using namespace drumlin;
#include "gstreamer.h"
#include "server.h"
#include "source.h"

namespace Pleg {

///**
// * @brief PlegApplication::startBluetooth : bluetooth thread starter
// * @param task const char*
// * @return Bluetooth*
// */
//Bluetooth *PlegApplication::startBluetooth(const char* task)
//{
//    Thread *thread = new Thread(task);
//    Bluetooth *bluet = new Bluetooth(thread);
//    Debug() << "starting Bluetooth thread" << task << bluet << thread;
//    app->addThread(thread,true);
//    return bluet;
//}

void PlegApplication::startMock()
{
    Sources::MockSource *mock(new Sources::MockSource());
    Sources::sources.add("mock",mock);
    mock->start();
}

/**
 * @brief PlegApplication::startGstreamer : gstreamer thread starter
 * @param task const char*
 * @return GStreamer*
 */
GStreamer::GStreamer *PlegApplication::startGStreamer(const char* task)
{
    Thread *thread = new Thread(task);
    GStreamer::GStreamer *gst = GStreamer::GStreamer::getInstance(thread);
    if(!gst->getThread()->isStarted()){
        Debug() << "starting GStreamer thread" << task << gst << thread;
        app->addThread(thread);
    }
    return gst;
}

bool PlegApplication::event(Event *pevent)
{
    do{
        switch(pevent->type()){
        case ApplicationShutdown:
        {
            if(main_server)main_server->writeLog();
            return false;
        }
//        case EventType::BluetoothStartThread:
//        {
//            string mac(pod_event_cast<string>(pevent)->getVal());
//            string task = string("source:")+mac;
//            Bluetooth *worker = startBluetooth(task.c_str());
//            while(!worker->getThread()->isStarted())
//                boost::this_thread::yield();
//            regex rx("([a-fA-F0-9]{2}:?){6}");
//            if(regex_match(mac,rx) || mac == "mock" || mac == "all")
//                make_pod_event(EventType::BluetoothConnectDevices,"connectDevices",mac)->send(worker->getThread());
//            break;
//        }
        default:
            return false;
        }
        return true;
    }while(false);
    return false;
}

} // namespace Pleg

Pleg::PlegApplication *app;
