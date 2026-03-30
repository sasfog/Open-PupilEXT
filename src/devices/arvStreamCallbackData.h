#pragma once

#ifndef USE_PYLON

// NOTE: has to happen, because aravis includes glib-2.0, and there
//  the definition "signals" is clashing with the Qt definition
#undef signals
#include <arv.h>
#include <arvbuffer.h>
#define signals Q_SIGNALS

typedef struct {
    ArvStream *stream;
    int counter;
    gboolean done;
    void* emitter; // has to use void* instead of SingleCameraImageEventHandler* to avoid circular referencing
    bool aboutToStopGrabbing;
    char cameraContext;
} ArvStreamCallbackData;

#endif
