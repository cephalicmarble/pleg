#ifndef COMPAT_H_
#define COMPAT_H_

#include "../drumlin/metatypes.h"

#define ThreadTypes (\
    ThreadType_first,\
    ThreadType_http,\
    ThreadType_bluez,\
    ThreadType_source,\
    ThreadType_gstreamer,\
    ThreadType_transform,\
    ThreadType_terminator,\
    ThreadType_test,\
    ThreadType_last\
)
ENUM(ThreadType,ThreadTypes)

#endif // COMPAT_H_
