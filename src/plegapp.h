#ifndef PLEGAPP_H
#define PLEGAPP_H

#include "drumlin/application.h"
#include "drumlin/metatypes.h"
#include "drumlin/object.h"
using namespace drumlin;

/**
 * @brief The Type enum from QEventType::User to QEventType::MaxUser
 */
#define EventTypes (\
    None,\
    Event_first,\
    Thread_first,\
    ThreadWork,\
    ThreadWarning,\
    ThreadShutdown,\
    ThreadRemove,\
    Thread_last,\
\
    ApplicationClose,\
    ApplicationThreadsGone,\
    ApplicationShutdown,\
    ApplicationRestart,\
    DrumlinEvent_last,\
\
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
    Transform_first,\
    TransformError,\
    Transform_last,\
\
    TracerBlock,\
\
    Event_last\
)
ENUM(EventType,EventTypes)

namespace Pleg {

class Bluetooth;

namespace GStreamer {
    class GStreamer;
}

class PlegApplication
    : public Object, public drumlin::Application
{
public:
    PlegApplication(int,char**){}
//    Pleg::Bluetooth *startBluetooth(const char* task);
    void startMock();
    Pleg::GStreamer::GStreamer *startGStreamer(const char* task);
    bool event(Event *);
};

} // namespace Pleg

extern Pleg::PlegApplication *app;

#endif // PLEGAPP_H
