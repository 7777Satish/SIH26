#ifndef RENDERER_H
#define RENDERER_H

#include <SDL3/SDL.h>
#include "types.h"

SDL_Texture *Renderer_CreateFeedTexture(SDL_Renderer *renderer);
void Renderer_DestroyFeedTexture(SDL_Texture *texture);
void Renderer_DrawDashboard(SDL_Renderer *renderer, SDL_Texture *feed_texture, const FrameBuffer_t *frame, SimConfig_t *config, SimState_t *state, const CVResult_t *cv, const ProfilerState_t *profiler, float fps);

#endif
