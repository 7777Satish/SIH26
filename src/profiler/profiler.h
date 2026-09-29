#ifndef PROFILER_H
#define PROFILER_H

#include "types/types.h"

typedef enum ProfilerLayer {
    PROFILER_VIDEO = 0,
    PROFILER_CV,
    PROFILER_RENDERING,
    PROFILER_UI
} ProfilerLayer_t;

void Profiler_BeginFrame(ProfilerState_t *profiler);
void Profiler_Start(ProfilerState_t *profiler, ProfilerLayer_t layer);
void Profiler_End(ProfilerState_t *profiler, ProfilerLayer_t layer);
void Profiler_EndFrame(ProfilerState_t *profiler);
const char *Profiler_LayerName(ProfilerLayer_t layer);

#define PROFILE_START(profiler, layer_id) Profiler_Start((profiler), (layer_id))
#define PROFILE_END(profiler, layer_id) Profiler_End((profiler), (layer_id))

#endif
