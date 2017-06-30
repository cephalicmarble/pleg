#ifndef PLEGAPP_H
#define PLEGAPP_H

namespace GStreamer {
    class QSGStreamer;
}

namespace Pleg {

class Bluetooth;

class PlegApplication
{
public:
    PlegApplication();
    Bluetooth *startBluetooth(const char* task);
#if defined(GSTREAMER)
    GStreamer::QSGStreamer *startGStreamer(const char* task);
#endif
};

} // namespace Pleg

#endif // PLEGAPP_H
