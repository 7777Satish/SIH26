#ifndef VIDEO_GEN_H
#define VIDEO_GEN_H

#include "types.h"

void VideoGen_UpdateTarget(const SimConfig_t *config, SimState_t *state, float delta_seconds);
void VideoGen_Generate(const SimConfig_t *config, const SimState_t *state, FrameBuffer_t *frame);

#endif
