#include "profiler.h"
#include <SDL3/SDL.h>
#include <string.h>

void Profiler_BeginFrame(ProfilerState_t *profiler)
{
    memset(profiler->start_ns, 0, sizeof(profiler->start_ns));
    profiler->frame_ns = SDL_GetTicksNS();
}

void Profiler_Start(ProfilerState_t *profiler, ProfilerLayer_t layer)
{
    if (layer >= 0 && layer < PROFILER_LAYER_COUNT) {
        profiler->start_ns[layer] = SDL_GetTicksNS();
    }
}

void Profiler_End(ProfilerState_t *profiler, ProfilerLayer_t layer)
{
    const uint64_t now = SDL_GetTicksNS();
    if (layer >= 0 && layer < PROFILER_LAYER_COUNT && profiler->start_ns[layer] != 0) {
        const uint64_t elapsed = now - profiler->start_ns[layer];
        profiler->rolling_ns[layer] = (profiler->rolling_ns[layer] * 7u + elapsed) / 8u;
        profiler->milliseconds[layer] = (float)profiler->rolling_ns[layer] / 1000000.0f;
    }
}

void Profiler_EndFrame(ProfilerState_t *profiler)
{
    const uint64_t now = SDL_GetTicksNS();
    profiler->frame_ns = now - profiler->frame_ns;
    for (int i = 0; i < PROFILER_LAYER_COUNT; ++i) {
        profiler->cpu_percent[i] = profiler->frame_ns > 0
            ? (float)((double)profiler->rolling_ns[i] * 100.0 / (double)profiler->frame_ns)
            : 0.0f;
    }
}

const char *Profiler_LayerName(ProfilerLayer_t layer)
{
    static const char *names[PROFILER_LAYER_COUNT] = { "VIDEO GEN", "CV TRACKER", "RENDERING", "UI" };
    return layer >= 0 && layer < PROFILER_LAYER_COUNT ? names[layer] : "UNKNOWN";
}
