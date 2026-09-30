#ifndef VIDEO_SOURCE_H
#define VIDEO_SOURCE_H

#include <SDL3/SDL.h>
#include <stdio.h>
#include "types/types.h"

#define VIDEO_SOURCE_PATH_LENGTH 1024

typedef struct VideoSource {
    FILE *process;
    SDL_Mutex *mutex;
    char path[VIDEO_SOURCE_PATH_LENGTH];
    char pending_path[VIDEO_SOURCE_PATH_LENGTH];
    bool pending;
    bool active;
    bool failed;
    bool reported_first_frame;
} VideoSource_t;

void VideoSource_Initialize(VideoSource_t *source);
void VideoSource_Shutdown(VideoSource_t *source);
void VideoSource_RequestPath(VideoSource_t *source, const char *path);
void VideoSource_RequestOpen(SDL_Window *window, VideoSource_t *source);
bool VideoSource_ApplyPending(VideoSource_t *source);
bool VideoSource_ReadFrame(VideoSource_t *source, EnvironmentFrame_t *environment, FrameBuffer_t *camera, float camera_center_x, float camera_center_y);
void VideoSource_Close(VideoSource_t *source);
const char *VideoSource_GetPath(const VideoSource_t *source);

#endif