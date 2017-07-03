#ifndef PLEGAPP_H
#define PLEGAPP_H

#include "application.h"

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
    GStreamer::QSGStreamer *startGStreamer(const char* task);
};

extern drumlin::Application<PlegApplication> *app;

} // namespace Pleg

#endif // PLEGAPP_H
