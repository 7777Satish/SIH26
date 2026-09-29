#ifndef CV_TRACKER_H
#define CV_TRACKER_H

#include "types/types.h"

void CVTracker_Reset(void);
void CVTracker_SetExternalPrediction(float x, float y, float confidence);
CVResult_t CVTracker_Process(const SimConfig_t *config, const FrameBuffer_t *frame, float delta_seconds, float latency_seconds);

#endif
