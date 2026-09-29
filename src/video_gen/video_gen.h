#ifndef VIDEO_GEN_H
#define VIDEO_GEN_H

#include "types/types.h"

void VideoGen_UpdateTarget(const SimConfig_t *config, SimState_t *state, float delta_seconds);
void VideoGen_ApplyCameraJitter(const SimConfig_t *config, SimState_t *state);
void VideoGen_MoveTarget(SimState_t *state, float delta_x, float delta_y);
void VideoGen_Generate(const SimConfig_t *config, const SimState_t *state, FrameBuffer_t *frame);

#endif
