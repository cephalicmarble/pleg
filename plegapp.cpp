#include "plegapp.h"

#include "thread.h"
using namespace drumlin;

namespace Pleg {

PlegApplication::PlegApplication()
{

}

/**
 * @brief PlegApplication::startBluetooth : bluetooth thread starter
 * @param task const char*
 * @return Bluetooth*
 */
Bluetooth *PlegApplication::startBluetooth(const char* task)
{
    Thread *thread = new Thread(task);
    Bluetooth *bluet = new Bluetooth(thread);
    Debug() << "starting Bluetooth thread" << task << bluet << thread;
    app->addThread(thread,true);
    return bluet;
}

/**
 * @brief PlegApplication::startGstreamer : gstreamer thread starter
 * @param task const char*
 * @return GStreamer*
 */
GStreamer::QSGStreamer *PlegApplication::startGStreamer(const char* task)
{
    Thread *thread = new Thread(task);
    GStreamer::QSGStreamer *gst = new GStreamer::QSGStreamer(thread);
    Debug() << "starting GStreamer thread" << task << gst << thread;
    app->addThread(thread,true);
    return gst;
}

Application<PlegApplication> *app;

} // namespace Pleg
