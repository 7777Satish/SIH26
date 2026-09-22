#ifndef CV_TRACKER_H
#define CV_TRACKER_H

#include "types.h"

void CVTracker_Reset(void);
CVResult_t CVTracker_Process(const SimConfig_t *config, const FrameBuffer_t *frame, float delta_seconds);

#endif
