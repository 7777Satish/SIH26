#include "renderer.h"
#include "gui.h"
#include "profiler.h"
#include <stdio.h>

static void panel(SDL_Renderer *renderer, int x, int y, int w, int h)
{
    GUI_SetColor(renderer, (SDL_Color){ 13, 25, 34, 255 });
    SDL_RenderFillRect(renderer, &(SDL_FRect){ (float)x, (float)y, (float)w, (float)h });
    GUI_SetColor(renderer, (SDL_Color){ 35, 64, 76, 255 });
    SDL_RenderRect(renderer, &(SDL_FRect){ (float)x, (float)y, (float)w, (float)h });
}

SDL_Texture *Renderer_CreateFeedTexture(SDL_Renderer *renderer)
{
    return SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STREAMING, FRAME_WIDTH, FRAME_HEIGHT);
}

void Renderer_DestroyFeedTexture(SDL_Texture *texture)
{
    SDL_DestroyTexture(texture);
}

void Renderer_DrawDashboard(SDL_Renderer *renderer, SDL_Texture *feed_texture, const FrameBuffer_t *frame, SimConfig_t *config, SimState_t *state, const CVResult_t *cv, const ProfilerState_t *profiler, float fps)
{
    char text[128];
    SDL_UpdateTexture(feed_texture, NULL, frame->pixels, FRAME_WIDTH);
    GUI_SetColor(renderer, (SDL_Color){ 7, 16, 23, 255 });
    SDL_RenderClear(renderer);
    GUI_SetColor(renderer, (SDL_Color){ 117, 225, 218, 255 });
    SDL_RenderDebugText(renderer, 22.0f, 18.0f, "ORBITAL EYE / VIRTUAL CAMERA TRACKING");
    GUI_SetColor(renderer, (SDL_Color){ 100, 121, 132, 255 });
    SDL_RenderDebugText(renderer, 22.0f, 38.0f, "LIVE SIMULATION  //  2000 x 2000 GLOBAL SPACE  //  SDL3 CPU PIPELINE");

    GUI_Begin();
    panel(renderer, 18, 66, 250, 718);
    GUI_Label(renderer, 34, 84, "SYSTEM CONTROLS");
    GUI_Label(renderer, 34, 115, "SIGNAL DISTURBANCE");
    GUI_Slider(renderer, 34, 138, 170, "NOISE", &config->noise_intensity, 0.0f, 100.0f);
    GUI_Slider(renderer, 34, 180, 170, "HAZE", &config->haze_level, 0.0f, 1.0f);
    GUI_Toggle(renderer, 34, 225, "GAUSSIAN", &config->gaussian_noise);
    GUI_Toggle(renderer, 34, 253, "SALT / PEPPER", &config->salt_pepper_noise);
    GUI_Label(renderer, 34, 300, "PTZ CONSTRAINTS");
    GUI_Slider(renderer, 34, 323, 170, "MAX PAN", &config->max_pan_speed, 1.0f, 12.0f);
    GUI_Slider(renderer, 34, 365, 170, "MAX TILT", &config->max_tilt_speed, 1.0f, 12.0f);
    GUI_Label(renderer, 34, 415, "TARGET MOTION");
    if (GUI_Button(renderer, 34, 440, 200, 30, "LINEAR PATH")) config->motion_pattern = MOTION_LINEAR;
    if (GUI_Button(renderer, 34, 478, 200, 30, "CIRCULAR ORBIT")) config->motion_pattern = MOTION_CIRCULAR;
    if (GUI_Button(renderer, 34, 516, 200, 30, "RANDOM WALK")) config->motion_pattern = MOTION_RANDOM;
    snprintf(text, sizeof(text), "PATTERN  %s", config->motion_pattern == MOTION_LINEAR ? "LINEAR" : (config->motion_pattern == MOTION_CIRCULAR ? "CIRCULAR" : "RANDOM"));
    GUI_Label(renderer, 34, 570, text);
    snprintf(text, sizeof(text), "PAN %+.1f deg/s   TILT %+.1f deg/s", state->pan_velocity, state->tilt_velocity);
    GUI_Label(renderer, 34, 600, text);
    GUI_Label(renderer, 34, 640, "CONTROLLER ONLINE");

    panel(renderer, 286, 66, 500, 500);
    GUI_Label(renderer, 304, 84, "GLOBAL ENVIRONMENT / 2000M");
    GUI_SetColor(renderer, (SDL_Color){ 19, 45, 52, 255 });
    SDL_RenderFillRect(renderer, &(SDL_FRect){ 286, 104, 500, 442 });
    GUI_SetColor(renderer, (SDL_Color){ 65, 101, 117, 255 });
    SDL_RenderRect(renderer, &(SDL_FRect){ 286, 104, 500, 442 });
    const float map_scale = 0.25f;
    SDL_FRect target = { 286.0f + state->true_target_x * map_scale - 4.0f, 104.0f + state->true_target_y * map_scale - 4.0f, 8.0f, 8.0f };
    GUI_SetColor(renderer, (SDL_Color){ 255, 195, 80, 255 });
    SDL_RenderFillRect(renderer, &target);
    SDL_FRect fov = { 286.0f + (state->camera_pan - 320.0f) * map_scale, 104.0f + (state->camera_tilt - 240.0f) * map_scale, 160.0f, 120.0f };
    GUI_SetColor(renderer, (SDL_Color){ 117, 225, 218, 255 });
    SDL_RenderRect(renderer, &fov);
    GUI_Label(renderer, 304, 520, "AMBER TRUE TARGET     CYAN ACTIVE FOV");

    panel(renderer, 806, 66, 664, 500);
    GUI_Label(renderer, 824, 84, "CAMERA PERSPECTIVE / 640 x 480");
    SDL_RenderTexture(renderer, feed_texture, NULL, &(SDL_FRect){ 818, 104, 640, 480 });
    GUI_SetColor(renderer, (SDL_Color){ 117, 225, 218, 255 });
    SDL_RenderLine(renderer, 1138, 104, 1138, 584);
    SDL_RenderLine(renderer, 818, 344, 1458, 344);
    if (cv->detected) SDL_RenderRect(renderer, &(SDL_FRect){ 818.0f + cv->centroid_x - 8.0f, 104.0f + cv->centroid_y - 8.0f, 16.0f, 16.0f });
    GUI_Label(renderer, 824, 600, "BRIGHTNESS THRESHOLD 200 // CENTROID LOCK");

    panel(renderer, 286, 584, 1184, 200);
    GUI_Label(renderer, 304, 602, "TELEMETRY");
    snprintf(text, sizeof(text), "FPS %6.1f   ACQUISITION %6.2f ms   TRACKING ERROR %6.1f px   BRIGHT PIXELS %d", fps, profiler->milliseconds[PROFILER_VIDEO], cv->pixel_error, cv->bright_pixels);
    GUI_Label(renderer, 304, 630, text);
    for (int i = 0; i < PROFILER_LAYER_COUNT; ++i) {
        snprintf(text, sizeof(text), "%-12s %7.3f ms   %5.1f%% CPU", Profiler_LayerName((ProfilerLayer_t)i), profiler->milliseconds[i], profiler->cpu_percent[i]);
        GUI_Label(renderer, 304, 662 + i * 22, text);
    }
    SDL_RenderPresent(renderer);
}
