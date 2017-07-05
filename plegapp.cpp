#include "plegapp.h"

#include "thread.h"
using namespace drumlin;
#include <boost/regex.hpp>
using namespace boost;
#include "gstreamer.h"
#include "application.h"
#include "event.h"

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

/**
 * @brief PlegApplication::startGstreamer : gstreamer thread starter
 * @param task const char*
 * @return GStreamer*
 */
GStreamer::GStreamer *PlegApplication::startGStreamer(const char* task)
{
    Thread *thread = new Thread(task);
    GStreamer::GStreamer *gst = new GStreamer::GStreamer(thread);
    Debug() << "starting GStreamer thread" << task << gst << thread;
    app->addThread(thread);
    return gst;
}

bool PlegApplication::event(Event *pevent)
{
    do{
        switch(pevent->type()){
//        case Event::Type::BluetoothStartThread:
//        {
//            string mac(pod_event_cast<string>(pevent)->getVal());
//            string task = string("source:")+mac;
//            Bluetooth *worker = startBluetooth(task.c_str());
//            while(!worker->getThread()->isStarted())
//                this_thread::yield();
//            regex rx("([a-fA-F0-9]{2}:?){6}");
//            if(regex_match(mac,rx) || mac == "mock" || mac == "all")
//                make_pod_event(Event::Type::BluetoothConnectDevices,"connectDevices",mac)->send(worker->getThread());
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

Application<Pleg::PlegApplication> *app;
