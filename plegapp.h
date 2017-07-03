#ifndef PLEGAPP_H
#define PLEGAPP_H

#include "object.h"
#include "application.h"
using namespace drumlin;

namespace GStreamer {
    class QSGStreamer;
}

namespace Pleg {

class Bluetooth;

class PlegApplication
    : public Object
{
public:
    PlegApplication();
    Bluetooth *startBluetooth(const char* task);
    GStreamer::QSGStreamer *startGStreamer(const char* task);
    bool event(Event *);
};

extern drumlin::Application<PlegApplication> *app;

} // namespace Pleg

#endif // PLEGAPP_H
