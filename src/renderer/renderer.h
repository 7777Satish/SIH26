#ifndef RENDERER_H
#define RENDERER_H

#include <SDL3/SDL.h>
#include "types/types.h"
#include "video_source/video_source.h"

SDL_Texture *Renderer_CreateFeedTexture(SDL_Renderer *renderer);
SDL_Texture *Renderer_CreateEnvironmentTexture(SDL_Renderer *renderer);
void Renderer_DestroyFeedTexture(SDL_Texture *texture);
void Renderer_DestroyEnvironmentTexture(SDL_Texture *texture);
void Renderer_DrawDashboard(SDL_Renderer *renderer, SDL_Window *window, SDL_Texture *feed_texture, SDL_Texture *environment_texture, const FrameBuffer_t *frame, const EnvironmentFrame_t *environment, SimConfig_t *config, SimState_t *state, const CVResult_t *cv, const ProfilerState_t *profiler, VideoSource_t *video_source, float fps);

#endif
