#ifndef PLEGAPP_H
#define PLEGAPP_H

#include "object.h"
#include "metatypes.h"
using namespace drumlin;

/**
 * @brief The Type enum from QEvent::Type::User to QEvent::Type::MaxUser
 */
#define EventTypes (None,\
    first,\
    Request_first,\
    Request_close,\
    RequestResponse,\
    Request_last,\
    Bluetooth_first,\
    BluetoothStartThread,\
    BluetoothConnectDevices,\
    BluetoothConnectSources,\
    BluetoothDeviceDisconnected,\
    BluetoothDeviceCheckConnected,\
    BluetoothSourceBadWrite,\
    BluetoothSourceDisconnected,\
    BluetoothSourceWriteConfig,\
    BluetoothSocketWriteConfig,\
    Bluetooth_last,\
\
    Gst_first,\
    GstConnectDevices,\
    GstConnectPipeline,\
    GstStreamPort,\
    GstStreamFile,\
    GstStreamEnd,\
    GstRemoveJob,\
    Gst_last,\
\
    Control_first,\
    ControlTest,\
    ControlStartStream,\
    ControlStreamStarted,\
    ControlOutputFrame,\
    Control_last,\
\
    Transform_first,\
    TransformError,\
    Transform_last,\
\
    TracerBlock,\
\
    Thread_first,\
    ThreadWarning,\
    ThreadShutdown,\
    ThreadRemove,\
    Thread_last,\
\
    ApplicationClose,\
    ApplicationThreadsGone,\
    ApplicationShutdown,\
    ApplicationRestart,\
\
    last\
)
ENUM(EventType,EventTypes)

namespace drumlin {
    template <class DrumlinApplication>
    class Application;
}

namespace Pleg {

class Bluetooth;

namespace GStreamer {
    class GStreamer;
}

class PlegApplication
    : public Object
{
public:
    PlegApplication(int,char**){}
//    Pleg::Bluetooth *startBluetooth(const char* task);
    Pleg::GStreamer::GStreamer *startGStreamer(const char* task);
    bool event(Event *);
};

} // namespace Pleg

extern drumlin::Application<Pleg::PlegApplication> *app;

#endif // PLEGAPP_H
