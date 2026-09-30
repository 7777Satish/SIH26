#include "video_source/video_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

static void video_file_callback(void *userdata, const char * const *files, int filter)
{
    (void)filter;
    VideoSource_t *source = userdata;
    if (source == NULL || files == NULL || files[0] == NULL) return;
    SDL_LockMutex(source->mutex);
    snprintf(source->pending_path, sizeof(source->pending_path), "%s", files[0]);
    source->pending = true;
    SDL_UnlockMutex(source->mutex);
}

static void set_pending_path(VideoSource_t *source, const char *path)
{
    while (*path == ' ' || *path == '\t') ++path;
    char normalized_path[VIDEO_SOURCE_PATH_LENGTH];
    snprintf(normalized_path, sizeof(normalized_path), "%s", path);
    size_t length = strlen(normalized_path);
    while (length > 0 && (normalized_path[length - 1] == '\n' || normalized_path[length - 1] == '\r')) {
        normalized_path[--length] = '\0';
    }
    if (length == 0) return;
    SDL_LockMutex(source->mutex);
    snprintf(source->pending_path, sizeof(source->pending_path), "%s", normalized_path);
    source->pending = true;
    SDL_UnlockMutex(source->mutex);
}

static bool request_windows_picker(VideoSource_t *source)
{
    const char *command =
        "powershell.exe -NoProfile -STA -Command "
        "\"Add-Type -AssemblyName System.Windows.Forms; "
        "\\$dialog = New-Object System.Windows.Forms.OpenFileDialog; "
        "\\$dialog.Filter = 'Video files|*.mp4;*.mov;*.mkv;*.avi;*.webm;*.mpeg;*.mpg|All files|*.*'; "
        "if (\\$dialog.ShowDialog() -eq 'OK') { \\$dialog.FileName }\"";
    FILE *picker = popen(command, "r");
    if (picker == NULL) return false;
    char windows_path[VIDEO_SOURCE_PATH_LENGTH];
    const bool selected = fgets(windows_path, sizeof(windows_path), picker) != NULL;
    pclose(picker);
    if (!selected) return false;

    char *path = windows_path;
    if (strlen(path) >= 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        static char wsl_path[VIDEO_SOURCE_PATH_LENGTH];
        const char drive = path[0] >= 'A' && path[0] <= 'Z' ? (char)(path[0] - 'A' + 'a') : path[0];
        snprintf(wsl_path, sizeof(wsl_path), "/mnt/%c/%.*s", drive, VIDEO_SOURCE_PATH_LENGTH - 8, path + 3);
        for (size_t index = 0; wsl_path[index] != '\0'; ++index) {
            if (wsl_path[index] == '\\') wsl_path[index] = '/';
        }
        set_pending_path(source, wsl_path);
    } else {
        set_pending_path(source, path);
    }
    return true;
}

static bool shell_quote_path(const char *path, char *quoted, size_t quoted_size)
{
    size_t output = 0;
    if (quoted_size < 3) return false;
    quoted[output++] = '\'';
    for (size_t index = 0; path[index] != '\0'; ++index) {
        if (path[index] == '\'') {
            if (output + 4 >= quoted_size) return false;
            quoted[output++] = '\'';
            quoted[output++] = '\\';
            quoted[output++] = '\'';
            quoted[output++] = '\'';
        } else {
            if (output + 2 >= quoted_size) return false;
            quoted[output++] = path[index];
        }
    }
    quoted[output++] = '\'';
    quoted[output] = '\0';
    return true;
}

static bool open_decoder(VideoSource_t *source)
{
    char quoted_path[VIDEO_SOURCE_PATH_LENGTH * 2];
    char command[VIDEO_SOURCE_PATH_LENGTH * 2 + 512];
    if (!shell_quote_path(source->path, quoted_path, sizeof(quoted_path))) return false;
#ifdef _WIN32
    const char *null_device = "NUL";
#else
    const char *null_device = "/dev/null";
#endif
    snprintf(command, sizeof(command),
        "ffmpeg -hide_banner -loglevel error -re -stream_loop -1 -i %s "
        "-vf 'scale=2000:2000:force_original_aspect_ratio=decrease,pad=2000:2000:(ow-iw)/2:(oh-ih)/2:color=black' "
        "-f rawvideo -pix_fmt gray pipe:1 2>%s",
        quoted_path, null_device);
#ifdef _WIN32
    source->process = popen(command, "rb");
#else
    source->process = popen(command, "r");
#endif
    return source->process != NULL;
}

void VideoSource_Initialize(VideoSource_t *source)
{
    *source = (VideoSource_t){ 0 };
    source->mutex = SDL_CreateMutex();
}

void VideoSource_Shutdown(VideoSource_t *source)
{
    VideoSource_Close(source);
    SDL_DestroyMutex(source->mutex);
    source->mutex = NULL;
}

void VideoSource_RequestPath(VideoSource_t *source, const char *path)
{
    if (path != NULL) set_pending_path(source, path);
}

void VideoSource_RequestOpen(SDL_Window *window, VideoSource_t *source)
{
#ifndef _WIN32
    if (getenv("WSL_DISTRO_NAME") != NULL || getenv("WSL_INTEROP") != NULL) {
        if (request_windows_picker(source)) return;
    }
#endif
    static const SDL_DialogFileFilter filters[] = {
        { "Video files", "mp4;mov;mkv;avi;webm;mpeg;mpg" },
        { "All files", "*" }
    };
    SDL_ShowOpenFileDialog(video_file_callback, source, window, filters, 2, NULL, false);
}

bool VideoSource_ApplyPending(VideoSource_t *source)
{
    char pending_path[VIDEO_SOURCE_PATH_LENGTH];
    SDL_LockMutex(source->mutex);
    if (!source->pending) {
        SDL_UnlockMutex(source->mutex);
        return false;
    }
    source->pending = false;
    snprintf(pending_path, sizeof(pending_path), "%s", source->pending_path);
    SDL_UnlockMutex(source->mutex);
    VideoSource_Close(source);
    snprintf(source->path, sizeof(source->path), "%s", pending_path);
    source->active = open_decoder(source);
    source->failed = !source->active;
    source->reported_first_frame = false;
    fprintf(stderr, "[video] selected: %s\n[video] decoder: %s\n", source->path, source->active ? "started" : "failed to start");
    return source->active;
}

bool VideoSource_ReadFrame(VideoSource_t *source, EnvironmentFrame_t *environment, FrameBuffer_t *camera, float camera_center_x, float camera_center_y)
{
    if (!source->active || source->process == NULL) return false;
    environment->width = VIDEO_ENV_WIDTH;
    environment->height = VIDEO_ENV_HEIGHT;
    const size_t bytes_read = fread(environment->pixels, 1, VIDEO_ENV_PIXELS, source->process);
    if (bytes_read == VIDEO_ENV_PIXELS) {
        int left = (int)(camera_center_x - FRAME_WIDTH * 0.5f);
        int top = (int)(camera_center_y - FRAME_HEIGHT * 0.5f);
        if (left < 0) left = 0;
        if (top < 0) top = 0;
        if (left > VIDEO_ENV_WIDTH - FRAME_WIDTH) left = VIDEO_ENV_WIDTH - FRAME_WIDTH;
        if (top > VIDEO_ENV_HEIGHT - FRAME_HEIGHT) top = VIDEO_ENV_HEIGHT - FRAME_HEIGHT;
        camera->width = FRAME_WIDTH;
        camera->height = FRAME_HEIGHT;
        for (int row = 0; row < FRAME_HEIGHT; ++row) {
            memcpy(camera->pixels + row * FRAME_WIDTH, environment->pixels + (top + row) * VIDEO_ENV_WIDTH + left, FRAME_WIDTH);
        }
        if (!source->reported_first_frame) {
            fprintf(stderr, "[video] first environment frame decoded; camera crop origin: %d,%d\n", left, top);
            source->reported_first_frame = true;
        }
        return true;
    }
    source->failed = true;
    fprintf(stderr, "[video] decoder produced no complete environment frame\n");
    VideoSource_Close(source);
    return false;
}

void VideoSource_Close(VideoSource_t *source)
{
    if (source->process != NULL) {
        pclose(source->process);
        source->process = NULL;
    }
    source->active = false;
}

const char *VideoSource_GetPath(const VideoSource_t *source)
{
    return source->path[0] != '\0' ? source->path : "No video selected";
}