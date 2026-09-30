#include "renderer/renderer.h"
#include "gui/gui.h"
#include "profiler/profiler.h"
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

static uint8_t feed_rgba[FRAME_PIXELS * 4];
static uint8_t environment_rgba[VIDEO_ENV_PIXELS * 4];
static TTF_Font *poppins_font;
static SDL_Texture *icon_close;
static SDL_Texture *icon_minimize;
static SDL_Texture *icon_maximize;
static SDL_Texture *icon_chevron_down;
static SDL_Texture *icon_back;
static SDL_Texture *icon_forward;
static SDL_Texture *icon_restore;
static SDL_Texture *icon_reload;
static int last_target_position = -1;
static bool environment_view = true;
static bool fit_view = true;
static bool overlay_grid = true;
static bool overlay_box = true;
static bool overlay_error = true;
static bool timeline_playing = true;
static float timeline_position;
#define CHART_POINTS 96
static float chart_history[3][CHART_POINTS];
static int chart_samples;

static const char *const TARGET_POSITIONS[] = { "Center", "Upper Left", "Lower Right" };
static const char *const TARGET_SHAPES[] = { "Square 10x10 px", "Circle 10 px", "Wide 20x8 px" };
static const char *const MOTION_PATTERNS[] = { "Linear Path", "Circular Orbit", "Random Walk" };

typedef struct WorkspaceLayout {
    float width;
    float height;
    float inset;
    float gap;
    float header;
    float left;
    float center;
    float right;
    float main_y;
    float main_height;
    float center_x;
    float right_x;
} WorkspaceLayout_t;

static const SDL_Color COLOR_BACKGROUND = { 17, 8, 13, 255 };
static const SDL_Color COLOR_SURFACE = { 29, 14, 21, 255 };
static const SDL_Color COLOR_SURFACE_RAISED = { 35, 18, 27, 255 };
static const SDL_Color COLOR_BORDER = { 55, 31, 41, 255 };
static const SDL_Color COLOR_GRID = { 40, 22, 31, 255 };
static const SDL_Color COLOR_TEXT = { 206, 187, 193, 255 };
static const SDL_Color COLOR_MUTED = { 130, 101, 112, 255 };
static const SDL_Color COLOR_LIME = { 207, 238, 75, 255 };
static const SDL_Color COLOR_LIME_DIM = { 117, 126, 44, 255 };
static const SDL_Color COLOR_RED = { 229, 76, 72, 255 };
static const SDL_Color COLOR_YELLOW = { 239, 179, 40, 255 };

static float max_float(float a, float b)
{
    return a > b ? a : b;
}

static float min_float(float a, float b)
{
    return a < b ? a : b;
}

static WorkspaceLayout_t workspace_layout(SDL_Renderer *renderer)
{
    int width;
    int height;
    SDL_GetRenderOutputSize(renderer, &width, &height);
    WorkspaceLayout_t layout = { 0 };
    layout.width = (float)width;
    layout.height = (float)height;
    layout.inset = max_float(4.0f, layout.width * 0.0038f);
    layout.gap = 0.0f;
    layout.header = 0.0f;
    layout.left = layout.width * 0.242f;
    layout.right = layout.width * 0.230f;
    layout.center = layout.width - layout.inset * 2.0f - layout.left - layout.right;
    layout.main_y = 0.0f;
    layout.main_height = layout.height;
    layout.center_x = layout.inset + layout.left;
    layout.right_x = layout.center_x + layout.center;
    return layout;
}

static void panel(SDL_Renderer *renderer, SDL_FRect rectangle, SDL_Color fill)
{
    const float radius = min_float(10.0f, min_float(rectangle.w, rectangle.h) * 0.12f);
    GUI_SetColor(renderer, fill);
    for (int row = 0; row < (int)rectangle.h; ++row) {
        const float edge = row < radius ? radius - row : (row >= rectangle.h - radius ? row - (rectangle.h - radius - 1.0f) : 0.0f);
        const float inset = edge > 0.0f ? radius - sqrtf(radius * radius - edge * edge) : 0.0f;
        SDL_RenderLine(renderer, rectangle.x + inset, rectangle.y + row, rectangle.x + rectangle.w - inset, rectangle.y + row);
    }
    GUI_SetColor(renderer, COLOR_BORDER);
    SDL_FPoint points[37];
    const float pi = 3.14159265359f;
    int point_count = 0;
    const float centers_x[] = { rectangle.x + rectangle.w - radius, rectangle.x + rectangle.w - radius, rectangle.x + radius, rectangle.x + radius };
    const float centers_y[] = { rectangle.y + radius, rectangle.y + rectangle.h - radius, rectangle.y + rectangle.h - radius, rectangle.y + radius };
    const float start_angles[] = { -pi * 0.5f, 0.0f, pi * 0.5f, pi };
    for (int corner = 0; corner < 4; ++corner) {
        for (int step = 0; step <= 8; ++step) {
            const float angle = start_angles[corner] + (pi * 0.5f * step / 8.0f);
            points[point_count++] = (SDL_FPoint){ centers_x[corner] + cosf(angle) * radius, centers_y[corner] + sinf(angle) * radius };
        }
    }
    points[point_count] = points[0];
    SDL_RenderLines(renderer, points, point_count + 1);
}

static void section_panel(SDL_Renderer *renderer, SDL_FRect rectangle, SDL_Color fill)
{
    GUI_SetColor(renderer, fill);
    SDL_RenderFillRect(renderer, &rectangle);
    GUI_SetColor(renderer, COLOR_BORDER);
    SDL_RenderRect(renderer, &rectangle);
}

static SDL_Texture *load_icon(SDL_Renderer *renderer, const char *name)
{
    char path[128];
    snprintf(path, sizeof(path), "assets/icons/%s.png", name);
    SDL_Texture *texture = IMG_LoadTexture(renderer, path);
    if (texture != NULL) SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

static void draw_transport_icon(SDL_Renderer *renderer, float x, float y, int direction, bool active)
{
    const SDL_Color color = active ? COLOR_BACKGROUND : COLOR_TEXT;
    panel(renderer, (SDL_FRect){ x, y, 38.0f, 38.0f }, active ? COLOR_LIME : COLOR_SURFACE_RAISED);
    GUI_SetColor(renderer, color);
    if (direction == 0) {
        SDL_RenderFillRect(renderer, &(SDL_FRect){ x + 12.0f, y + 10.0f, 5.0f, 18.0f });
        SDL_RenderFillRect(renderer, &(SDL_FRect){ x + 21.0f, y + 10.0f, 5.0f, 18.0f });
    } else {
        const float tip = direction < 0 ? x + 10.0f : x + 28.0f;
        const float base = direction < 0 ? x + 28.0f : x + 10.0f;
        SDL_RenderLine(renderer, base, y + 9.0f, base, y + 29.0f);
        SDL_RenderLine(renderer, base, y + 9.0f, tip, y + 19.0f);
        SDL_RenderLine(renderer, tip, y + 19.0f, base, y + 29.0f);
    }
}

static void draw_text(SDL_Renderer *renderer, float x, float y, const char *text, SDL_Color color, TTF_Font *font)
{
    if (font == NULL || text == NULL || text[0] == '\0') return;
    SDL_Surface *surface = TTF_RenderText_Blended(font, text, 0, color);
    if (surface == NULL) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture != NULL) {
        SDL_RenderTexture(renderer, texture, NULL, &(SDL_FRect){ x, y, (float)surface->w, (float)surface->h });
        SDL_DestroyTexture(texture);
    }
    SDL_DestroySurface(surface);
}

static void text_at(SDL_Renderer *renderer, float x, float y, const char *text, SDL_Color color)
{
    draw_text(renderer, x, y, text, color, poppins_font);
}

static void text_right(SDL_Renderer *renderer, float right, float y, const char *text, SDL_Color color)
{
    int width;
    int height;
    if (poppins_font == NULL || !TTF_GetStringSize(poppins_font, text, 0, &width, &height)) return;
    draw_text(renderer, right - (float)width, y, text, color, poppins_font);
}

static void section(SDL_Renderer *renderer, float x, float y, const char *title)
{
    draw_text(renderer, x, y, title, COLOR_MUTED, poppins_font);
}

static void divider(SDL_Renderer *renderer, float x, float y, float width)
{
    GUI_SetColor(renderer, COLOR_BORDER);
    SDL_RenderLine(renderer, x, y, x + width, y);
}

static void metric(SDL_Renderer *renderer, float x, float y, float width, const char *title, const char *value, const char *detail, bool warning)
{
    panel(renderer, (SDL_FRect){ x, y, width, 52.0f }, COLOR_SURFACE_RAISED);
    text_at(renderer, x + 10.0f, y + 10.0f, title, COLOR_MUTED);
    draw_text(renderer, x + width - 74.0f, y + 8.0f, value, COLOR_TEXT, poppins_font);
    text_at(renderer, x + 10.0f, y + 30.0f, detail, warning ? COLOR_RED : COLOR_MUTED);
}

static void record_chart_sample(const SimConfig_t *config, const CVResult_t *cv, const ProfilerState_t *profiler)
{
    const float sample[3] = {
        profiler->milliseconds[PROFILER_VIDEO],
        cv->confidence,
        config->noise_intensity
    };
    if (chart_samples == 0) {
        for (int row = 0; row < 3; ++row) {
            for (int point = 0; point < CHART_POINTS; ++point) chart_history[row][point] = sample[row];
        }
        chart_samples = CHART_POINTS;
        return;
    }
    for (int row = 0; row < 3; ++row) {
        for (int point = 1; point < CHART_POINTS; ++point) chart_history[row][point - 1] = chart_history[row][point];
        chart_history[row][CHART_POINTS - 1] = sample[row];
    }
}

static void draw_chart_row(SDL_Renderer *renderer, float x, float y, float width, float height, int row, float maximum, SDL_Color color)
{
    GUI_SetColor(renderer, COLOR_GRID);
    SDL_RenderLine(renderer, x, y + height, x + width, y + height);
    GUI_SetColor(renderer, color);
    SDL_FPoint points[CHART_POINTS];
    for (int point = 0; point < CHART_POINTS; ++point) {
        const float normalized = chart_history[row][point] / maximum;
        const float clamped = normalized < 0.0f ? 0.0f : (normalized > 1.0f ? 1.0f : normalized);
        points[point] = (SDL_FPoint){ x + width * point / (CHART_POINTS - 1), y + height - clamped * height };
    }
    SDL_RenderLines(renderer, points, CHART_POINTS);
}

static void reset_configuration(SimConfig_t *config, SimState_t *state)
{
    config->target_speed = 70.0f;
    config->max_pan_speed = 5.0f;
    config->max_tilt_speed = 5.0f;
    config->platform_jitter = 5.0f;
    config->noise_intensity = 18.0f;
    config->haze_level = 0.12f;
    config->gaussian_noise = true;
    config->salt_pepper_noise = true;
    config->poisson_noise = false;
    config->manual_target = false;
    config->kp = 0.025f;
    config->ki = 0.0002f;
    config->kd = 0.006f;
    config->motion_pattern = MOTION_CIRCULAR;
    config->target_mode = TARGET_SINGLE;
    config->target_position = TARGET_CENTER;
    config->target_shape = TARGET_SQUARE;
    last_target_position = -1;
    state->true_target_x = WORLD_SIZE * 0.5f;
    state->true_target_y = WORLD_SIZE * 0.5f;
    state->camera_pan = WORLD_SIZE * 0.5f;
    state->camera_tilt = WORLD_SIZE * 0.5f;
    state->pan_velocity = 0.0f;
    state->tilt_velocity = 0.0f;
    state->simulation_time = 0.0f;
    environment_view = true;
    fit_view = true;
    overlay_grid = true;
    overlay_box = true;
    overlay_error = true;
    timeline_playing = true;
    timeline_position = 0.0f;
    chart_samples = 0;
}

static void apply_target_position(const SimConfig_t *config, SimState_t *state)
{
    if (config->target_position == TARGET_UPPER_LEFT) {
        state->true_target_x = WORLD_SIZE * 0.25f;
        state->true_target_y = WORLD_SIZE * 0.25f;
    } else if (config->target_position == TARGET_LOWER_RIGHT) {
        state->true_target_x = WORLD_SIZE * 0.75f;
        state->true_target_y = WORLD_SIZE * 0.75f;
    } else {
        state->true_target_x = WORLD_SIZE * 0.5f;
        state->true_target_y = WORLD_SIZE * 0.5f;
    }
}

SDL_Texture *Renderer_CreateFeedTexture(SDL_Renderer *renderer)
{
    TTF_Init();
    poppins_font = TTF_OpenFont("assets/Poppins/Poppins-Regular.ttf", 15.0f);
    GUI_SetFonts(poppins_font, poppins_font);
    icon_close = load_icon(renderer, "close");
    icon_minimize = load_icon(renderer, "minus");
    icon_maximize = load_icon(renderer, "square");
    icon_chevron_down = load_icon(renderer, "chevron_down");
    icon_back = load_icon(renderer, "back");
    icon_forward = load_icon(renderer, "forward");
    icon_restore = load_icon(renderer, "restore");
    icon_reload = load_icon(renderer, "reload");
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, FRAME_WIDTH, FRAME_HEIGHT);
    if (texture != NULL) SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    return texture;
}

SDL_Texture *Renderer_CreateEnvironmentTexture(SDL_Renderer *renderer)
{
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, VIDEO_ENV_WIDTH, VIDEO_ENV_HEIGHT);
    if (texture != NULL) SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    return texture;
}

void Renderer_DestroyFeedTexture(SDL_Texture *texture)
{
    SDL_DestroyTexture(texture);
    SDL_DestroyTexture(icon_close);
    SDL_DestroyTexture(icon_minimize);
    SDL_DestroyTexture(icon_maximize);
    SDL_DestroyTexture(icon_chevron_down);
    SDL_DestroyTexture(icon_back);
    SDL_DestroyTexture(icon_forward);
    SDL_DestroyTexture(icon_restore);
    SDL_DestroyTexture(icon_reload);
    GUI_SetFonts(NULL, NULL);
    if (poppins_font != NULL) TTF_CloseFont(poppins_font);
    TTF_Quit();
}

void Renderer_DestroyEnvironmentTexture(SDL_Texture *texture)
{
    SDL_DestroyTexture(texture);
}

void Renderer_DrawDashboard(SDL_Renderer *renderer, SDL_Window *window, SDL_Texture *feed_texture, SDL_Texture *environment_texture, const FrameBuffer_t *frame, const EnvironmentFrame_t *environment, SimConfig_t *config, SimState_t *state, const CVResult_t *cv, const ProfilerState_t *profiler, VideoSource_t *video_source, float fps)
{
    char text[128];
    const bool custom_video = config->video_source_mode == VIDEO_SOURCE_CUSTOM && video_source->active;
    for (int i = 0; i < FRAME_PIXELS; ++i) {
        const uint8_t value = frame->pixels[i];
        feed_rgba[i * 4] = value;
        feed_rgba[i * 4 + 1] = value;
        feed_rgba[i * 4 + 2] = value;
        feed_rgba[i * 4 + 3] = 255;
    }
    SDL_UpdateTexture(feed_texture, NULL, feed_rgba, FRAME_WIDTH * 4);
    if (custom_video) {
        for (int i = 0; i < VIDEO_ENV_PIXELS; ++i) {
            const uint8_t value = environment->pixels[i];
            environment_rgba[i * 4] = value;
            environment_rgba[i * 4 + 1] = value;
            environment_rgba[i * 4 + 2] = value;
            environment_rgba[i * 4 + 3] = 255;
        }
        SDL_UpdateTexture(environment_texture, NULL, environment_rgba, VIDEO_ENV_WIDTH * 4);
    }

    const WorkspaceLayout_t layout = workspace_layout(renderer);
    const float pad = max_float(16.0f, min_float(layout.width, layout.height) * 0.021f);
    const float content_y = layout.main_y;
    const float center_x = layout.center_x;
    const float right_x = layout.right_x;
    const float rail_scale = min_float(1.14f, layout.main_height / 600.0f);
    const float timeline_height = min_float(220.0f, layout.main_height * 0.32f);
    const float camera_height = layout.main_height - timeline_height - layout.gap;
    if (last_target_position != (int)config->target_position) {
        apply_target_position(config, state);
        last_target_position = config->target_position;
    }
    if (timeline_playing) timeline_position += fps > 0.0f ? 1.0f / fps : 0.0f;
    if (timeline_position > 150.0f) timeline_position = 0.0f;
    if (timeline_playing) record_chart_sample(config, cv, profiler);

    GUI_SetColor(renderer, COLOR_BACKGROUND);
    SDL_RenderClear(renderer);
    GUI_Begin(renderer);
    const float left_x = layout.inset;
    const float left_inner = left_x + pad;
    const float left_width = layout.left - pad * 2.0f;
    section_panel(renderer, (SDL_FRect){ left_x, content_y, layout.left, layout.main_height }, COLOR_SURFACE);
    section(renderer, left_inner, content_y + 20.0f * rail_scale, "SIMULATION & TARGET");
    text_at(renderer, left_inner, content_y + 48.0f * rail_scale, "Target Mode", COLOR_MUTED);
        text_at(renderer, left_inner + left_width * 0.62f, content_y + 48.0f * rail_scale, config->target_mode == TARGET_SINGLE ? "Single Beacon" : "Multi Beacon", COLOR_TEXT);
        if (GUI_ChoiceButton(renderer, (int)left_inner, (int)(content_y + 72.0f * rail_scale), (int)(left_width * 0.5f), 26, "Single", config->target_mode == TARGET_SINGLE)) config->target_mode = TARGET_SINGLE;
        if (GUI_ChoiceButton(renderer, (int)(left_inner + left_width * 0.5f), (int)(content_y + 72.0f * rail_scale), (int)(left_width * 0.5f), 26, "Multi", config->target_mode == TARGET_MULTI)) config->target_mode = TARGET_MULTI;
    text_at(renderer, left_inner, content_y + 122.0f * rail_scale, "Target Shape", COLOR_MUTED);
    GUI_Dropdown(renderer, (int)(left_inner + left_width * 0.48f), (int)(content_y + 114.0f * rail_scale), (int)(left_width * 0.52f), 28, TARGET_SHAPES[config->target_shape], TARGET_SHAPES, 3, (int *)&config->target_shape, 3);
    text_at(renderer, left_inner, content_y + 188.0f * rail_scale, "Initial Position", COLOR_MUTED);
    GUI_Dropdown(renderer, (int)left_inner, (int)(content_y + 220.0f * rail_scale), (int)left_width, 32, TARGET_POSITIONS[config->target_position], TARGET_POSITIONS, 3, (int *)&config->target_position, 1);
    text_at(renderer, left_inner, content_y + 276.0f * rail_scale, "Motion Pattern", COLOR_MUTED);
    GUI_Dropdown(renderer, (int)left_inner, (int)(content_y + 304.0f * rail_scale), (int)left_width, 32, MOTION_PATTERNS[config->motion_pattern], MOTION_PATTERNS, 3, (int *)&config->motion_pattern, 2);
    divider(renderer, left_inner, content_y + 354.0f * rail_scale, left_width);
    section(renderer, left_inner, content_y + 378.0f * rail_scale, "DISTURBANCES & NOISE");
    if (custom_video) {
        text_at(renderer, left_inner, content_y + 418.0f * rail_scale, "Disabled for imported video", COLOR_MUTED);
        text_at(renderer, left_inner, content_y + 450.0f * rail_scale, "Source frames are used unchanged", COLOR_MUTED);
    } else {
        text_at(renderer, left_inner, content_y + 394.0f * rail_scale, "Image Noise", COLOR_MUTED);
        GUI_Slider(renderer, (int)left_inner, (int)(content_y + 418.0f * rail_scale), (int)left_width, "", &config->noise_intensity, 0.0f, 100.0f);
        GUI_Checkbox(renderer, (int)left_inner, (int)(content_y + 456.0f * rail_scale), "Salt&P", &config->salt_pepper_noise);
        GUI_Checkbox(renderer, (int)(left_inner + left_width * 0.46f), (int)(content_y + 456.0f * rail_scale), "Gauss", &config->gaussian_noise);
        GUI_Checkbox(renderer, (int)(left_inner + left_width * 0.82f), (int)(content_y + 456.0f * rail_scale), "Poisson", &config->poisson_noise);
        text_at(renderer, left_inner, content_y + 492.0f * rail_scale, "Atmosphere", COLOR_MUTED);
        text_at(renderer, left_inner + left_width * 0.82f, content_y + 492.0f * rail_scale, "Haze", COLOR_LIME);
        if (GUI_ChoiceButton(renderer, (int)left_inner, (int)(content_y + 516.0f * rail_scale), (int)(left_width / 3.0f), 26, "Clear", config->haze_level == 0.0f)) config->haze_level = 0.0f;
        if (GUI_ChoiceButton(renderer, (int)(left_inner + left_width / 3.0f), (int)(content_y + 516.0f * rail_scale), (int)(left_width / 3.0f), 26, "Haze", config->haze_level > 0.0f && config->haze_level < 0.4f)) config->haze_level = 0.12f;
        if (GUI_ChoiceButton(renderer, (int)(left_inner + left_width * 2.0f / 3.0f), (int)(content_y + 516.0f * rail_scale), (int)(left_width / 3.0f), 26, "Fog", config->haze_level >= 0.4f)) config->haze_level = 0.5f;
        GUI_Slider(renderer, (int)left_inner, (int)(content_y + 566.0f * rail_scale), (int)left_width, "Platform Jitter", &config->platform_jitter, 0.0f, 40.0f);
    }
    const int footer_y = (int)(content_y + layout.main_height - pad - 32.0f);
    if (GUI_Button(renderer, (int)left_inner, footer_y, (int)(left_width * 0.58f), 28, "Reset Configuration")) reset_configuration(config, state);
    GUI_Toggle(renderer, (int)(left_inner + left_width * 0.62f), footer_y + 4, "Manual", &config->manual_target);

    section_panel(renderer, (SDL_FRect){ center_x, content_y, layout.center, camera_height }, COLOR_SURFACE);
    panel(renderer, (SDL_FRect){ center_x + pad, content_y + pad, layout.center - pad * 2.0f, 42.0f }, COLOR_SURFACE_RAISED);
    if (GUI_ChoiceButton(renderer, (int)(center_x + pad + 12.0f), (int)(content_y + pad + 8.0f), 76, 26, "Camera", !environment_view)) environment_view = false;
    if (GUI_ChoiceButton(renderer, (int)(center_x + pad + 102.0f), (int)(content_y + pad + 8.0f), 88, 26, "Environment", environment_view)) environment_view = true;
    if (GUI_ChoiceButton(renderer, (int)(center_x + pad + 202.0f), (int)(content_y + pad + 8.0f), 64, 26, "Live", !custom_video)) config->video_source_mode = VIDEO_SOURCE_SYNTHETIC;
    if (GUI_ChoiceButton(renderer, (int)(center_x + pad + 274.0f), (int)(content_y + pad + 8.0f), 64, 26, "Video", custom_video)) {
        if (video_source->active) config->video_source_mode = VIDEO_SOURCE_CUSTOM;
        else VideoSource_RequestOpen(window, video_source);
    }
    if (GUI_Button(renderer, (int)(center_x + pad + 346.0f), (int)(content_y + pad + 8.0f), 72, 26, "Import")) VideoSource_RequestOpen(window, video_source);
    if (GUI_ChoiceButton(renderer, (int)(center_x + layout.center - 112.0f), (int)(content_y + pad + 8.0f), 42, 26, "100%", !fit_view)) fit_view = false;
    if (GUI_ChoiceButton(renderer, (int)(center_x + layout.center - 58.0f), (int)(content_y + pad + 8.0f), 42, 26, "Fit", fit_view)) fit_view = true;
    const float map_x = center_x + pad;
    const float map_y = content_y + 84.0f;
    const float map_width = layout.center - pad * 2.0f;
    const float map_height = camera_height - 98.0f;
    GUI_SetColor(renderer, (SDL_Color){ 20, 10, 16, 255 });
    SDL_RenderFillRect(renderer, &(SDL_FRect){ map_x, map_y, map_width, map_height });
    if (!environment_view) SDL_RenderTexture(renderer, feed_texture, NULL, &(SDL_FRect){ map_x, map_y, map_width, map_height });
    if (environment_view && custom_video) SDL_RenderTexture(renderer, environment_texture, NULL, &(SDL_FRect){ map_x, map_y, map_width, map_height });
    GUI_SetColor(renderer, COLOR_GRID);
    for (int grid = 1; grid < 10 && overlay_grid; ++grid) {
        const float x = map_x + map_width * grid / 10.0f;
        const float y = map_y + map_height * grid / 8.0f;
        SDL_RenderLine(renderer, x, map_y, x, map_y + map_height);
        if (grid < 8) SDL_RenderLine(renderer, map_x, y, map_x + map_width, y);
    }
    if (environment_view) {
        if (custom_video) {
            const float fit_scale = min_float(map_width / VIDEO_ENV_WIDTH, map_height / VIDEO_ENV_HEIGHT);
            const float map_scale = fit_view ? fit_scale : fit_scale * 1.25f;
            const float map_origin_x = map_x + (map_width - VIDEO_ENV_WIDTH * map_scale) * 0.5f;
            const float map_origin_y = map_y + (map_height - VIDEO_ENV_HEIGHT * map_scale) * 0.5f;
            const float camera_x = map_origin_x + (state->camera_pan - FRAME_WIDTH * 0.5f) * map_scale;
            const float camera_y = map_origin_y + (state->camera_tilt - FRAME_HEIGHT * 0.5f) * map_scale;
            const SDL_FRect camera_frame = { camera_x, camera_y, FRAME_WIDTH * map_scale, FRAME_HEIGHT * map_scale };
            GUI_SetColor(renderer, COLOR_LIME);
            SDL_RenderRect(renderer, &camera_frame);
            text_at(renderer, map_x + 10.0f, map_y + 10.0f, cv->detected ? "TRACKING LOCK" : "SEARCHING", cv->detected ? COLOR_LIME : COLOR_YELLOW);
        } else {
        const float fit_scale = min_float(map_width / WORLD_SIZE, map_height / WORLD_SIZE);
        const float map_scale = fit_view ? fit_scale : fit_scale * 1.25f;
        const float map_origin_x = map_x + (map_width - WORLD_SIZE * map_scale) * 0.5f;
        const float map_origin_y = map_y + (map_height - WORLD_SIZE * map_scale) * 0.5f;
        const float target_x = map_origin_x + state->true_target_x * map_scale;
        const float target_y = map_origin_y + state->true_target_y * map_scale;
        const float camera_x = map_origin_x + (state->camera_pan - FRAME_WIDTH * 0.5f) * map_scale;
        const float camera_y = map_origin_y + (state->camera_tilt - FRAME_HEIGHT * 0.5f) * map_scale;
        const SDL_FRect camera_frame = { camera_x, camera_y, FRAME_WIDTH * map_scale, FRAME_HEIGHT * map_scale };
        GUI_SetColor(renderer, COLOR_LIME_DIM);
        SDL_RenderRect(renderer, &camera_frame);
        GUI_SetColor(renderer, COLOR_LIME);
        SDL_RenderFillRect(renderer, &(SDL_FRect){ target_x - 9.0f, target_y - 9.0f, 18.0f, 18.0f });
        text_at(renderer, map_x + 10.0f, map_y + 10.0f, cv->detected ? "TRACKING LOCK" : "SEARCHING", cv->detected ? COLOR_LIME : COLOR_YELLOW);
        }
    }
    if (!environment_view && overlay_box && cv->detected) {
        const float detected_x = map_x + cv->centroid_x * map_width / FRAME_WIDTH;
        const float detected_y = map_y + cv->centroid_y * map_height / FRAME_HEIGHT;
        GUI_SetColor(renderer, COLOR_LIME);
        SDL_RenderRect(renderer, &(SDL_FRect){ detected_x - 12.0f, detected_y - 12.0f, 24.0f, 24.0f });
        if (overlay_error) SDL_RenderLine(renderer, map_x + map_width * 0.5f, map_y + map_height * 0.5f, detected_x, detected_y);
    }
    if (environment_view && custom_video && overlay_box && cv->detected) {
        const float fit_scale = min_float(map_width / VIDEO_ENV_WIDTH, map_height / VIDEO_ENV_HEIGHT);
        const float map_scale = fit_view ? fit_scale : fit_scale * 1.25f;
        const float map_origin_x = map_x + (map_width - VIDEO_ENV_WIDTH * map_scale) * 0.5f;
        const float map_origin_y = map_y + (map_height - VIDEO_ENV_HEIGHT * map_scale) * 0.5f;
        const float detected_x = map_origin_x + (state->camera_pan - FRAME_WIDTH * 0.5f + cv->centroid_x) * map_scale;
        const float detected_y = map_origin_y + (state->camera_tilt - FRAME_HEIGHT * 0.5f + cv->centroid_y) * map_scale;
        GUI_SetColor(renderer, COLOR_LIME);
        SDL_RenderRect(renderer, &(SDL_FRect){ detected_x - 8.0f, detected_y - 8.0f, 16.0f, 16.0f });
    }
    if (config->video_source_mode == VIDEO_SOURCE_CUSTOM && !custom_video) {
        text_at(renderer, map_x + 10.0f, map_y + 10.0f, "VIDEO LOAD FAILED - SELECT ANOTHER FILE", COLOR_RED);
    }

    section_panel(renderer, (SDL_FRect){ center_x, content_y + camera_height + layout.gap, layout.center, timeline_height }, COLOR_SURFACE);
    const float timeline_y = content_y + camera_height + layout.gap;
    const float timeline_pad = max_float(24.0f, pad);
    const float transport_y = timeline_y + 18.0f;
    const int timeline_seconds = (int)timeline_position;
    snprintf(text, sizeof(text), "%02d:%02d.%02d", timeline_seconds / 60, timeline_seconds % 60, (int)(timeline_position * 100.0f) % 100);
    text_at(renderer, center_x + timeline_pad, timeline_y + 24.0f, text, COLOR_LIME);
    text_at(renderer, center_x + timeline_pad + 88.0f, timeline_y + 24.0f, "/ 02:30.00", COLOR_MUTED);
    const float transport_center = center_x + layout.center * 0.5f;
    const float previous_x = transport_center - 66.0f;
    const float pause_x = transport_center - 19.0f;
    const float next_x = transport_center + 28.0f;
    if (GUI_Button(renderer, (int)previous_x, (int)transport_y, 38, 38, "")) timeline_position = 0.0f;
    if (GUI_Button(renderer, (int)pause_x, (int)transport_y, 38, 38, "")) timeline_playing = !timeline_playing;
    if (GUI_Button(renderer, (int)next_x, (int)transport_y, 38, 38, "")) timeline_position += 5.0f;
    draw_transport_icon(renderer, previous_x, transport_y, -1, false);
    draw_transport_icon(renderer, pause_x, transport_y, 0, timeline_playing);
    draw_transport_icon(renderer, next_x, transport_y, 1, false);
    text_at(renderer, center_x + layout.center - 210.0f, timeline_y + 24.0f, "Profile:", COLOR_MUTED);
    text_at(renderer, center_x + layout.center - 150.0f, timeline_y + 24.0f, "Harmonic Orbit", COLOR_TEXT);
    const float track_x = center_x + timeline_pad;
    const float track_y = timeline_y + 82.0f;
    const float track_width = layout.center - timeline_pad * 2.0f;
    text_at(renderer, track_x, track_y, "00:00              00:30              01:00              01:30              02:00              02:30", COLOR_MUTED);
    const float chart_x = track_x + 102.0f;
    const float chart_width = track_width - 102.0f;
    text_at(renderer, track_x, track_y + 28.0f, "Video Feed", COLOR_TEXT);
    text_at(renderer, track_x, track_y + 56.0f, "Tracking Lock", COLOR_TEXT);
    text_at(renderer, track_x, track_y + 84.0f, "Disturbance", COLOR_TEXT);
    draw_chart_row(renderer, chart_x, track_y + 20.0f, chart_width, 18.0f, 0, 10.0f, COLOR_LIME_DIM);
    draw_chart_row(renderer, chart_x, track_y + 48.0f, chart_width, 18.0f, 1, 1.0f, COLOR_LIME);
    draw_chart_row(renderer, chart_x, track_y + 76.0f, chart_width, 18.0f, 2, 100.0f, COLOR_YELLOW);
    const float progress_width = chart_width * (timeline_position / 150.0f);
    GUI_SetColor(renderer, COLOR_LIME);
    SDL_RenderLine(renderer, chart_x + progress_width, track_y + 4.0f, chart_x + progress_width, track_y + 108.0f);

    section_panel(renderer, (SDL_FRect){ right_x, content_y, layout.right, layout.main_height }, COLOR_SURFACE);
    const float right_inner = right_x + pad;
    const float right_width = layout.right - pad * 2.0f;
    section(renderer, right_inner, content_y + 20.0f * rail_scale, "CAMERA & SENSOR OPTICS");
    text_at(renderer, right_inner, content_y + 48.0f * rail_scale, "Sensor Res", COLOR_MUTED);
    text_right(renderer, right_inner + right_width, content_y + 48.0f * rail_scale, "640 x 480 px (FPA)", COLOR_TEXT);
    text_at(renderer, right_inner, content_y + 72.0f * rail_scale, "Virtual Screen", COLOR_MUTED);
    text_right(renderer, right_inner + right_width, content_y + 72.0f * rail_scale, "2000 x 2000 px", COLOR_MUTED);
    text_at(renderer, right_inner, content_y + 96.0f * rail_scale, "Camera FOV", COLOR_MUTED);
    text_right(renderer, right_inner + right_width, content_y + 96.0f * rail_scale, "4.0 x 3.0 deg", COLOR_TEXT);
    text_at(renderer, right_inner, content_y + 120.0f * rail_scale, "Frame Rate", COLOR_MUTED);
    snprintf(text, sizeof(text), "%.0f Hz", fps); text_right(renderer, right_inner + right_width, content_y + 120.0f * rail_scale, text, COLOR_LIME);
    GUI_Slider(renderer, (int)right_inner, (int)(content_y + 160.0f * rail_scale), (int)right_width, "Pan Limit", &config->max_pan_speed, 1.0f, 12.0f);
    GUI_Slider(renderer, (int)right_inner, (int)(content_y + 226.0f * rail_scale), (int)right_width, "Tilt Limit", &config->max_tilt_speed, 1.0f, 12.0f);
    divider(renderer, right_inner, content_y + 262.0f * rail_scale, right_width);
    section(renderer, right_inner, content_y + 284.0f * rail_scale, "VIEW OVERLAYS");
    GUI_Checkbox(renderer, (int)right_inner, (int)(content_y + 308.0f * rail_scale), "Alignment Grid", &overlay_grid);
    GUI_Checkbox(renderer, (int)right_inner, (int)(content_y + 332.0f * rail_scale), "Tracking Bounding Box", &overlay_box);
    GUI_Checkbox(renderer, (int)right_inner, (int)(content_y + 356.0f * rail_scale), "Error Vector", &overlay_error);
    divider(renderer, right_inner, content_y + 390.0f * rail_scale, right_width);
    section(renderer, right_inner, content_y + 400.0f * rail_scale, "REAL-TIME METRICS");
    snprintf(text, sizeof(text), "%.0f%%", cv->confidence * 100.0f); metric(renderer, right_inner, content_y + 432.0f * rail_scale, right_width, "Detection Confidence", text, cv->detected ? "Nominal" : "Searching", !cv->detected);
    snprintf(text, sizeof(text), "%.1f px", cv->pixel_error); metric(renderer, right_inner, content_y + 492.0f * rail_scale, right_width, "Tracking Error", text, cv->pixel_error < 10.0f ? "Nominal" : "Outside tolerance", cv->pixel_error >= 10.0f);
    snprintf(text, sizeof(text), "%.1f ms", profiler->milliseconds[PROFILER_CV]); metric(renderer, right_inner, content_y + 552.0f * rail_scale, right_width, "Acquisition", text, "Fast lock", false);
    text_at(renderer, right_inner, content_y + layout.main_height - 28.0f, custom_video ? "Custom Video" : (cv->detected ? "Active Lock" : "Searching"), COLOR_LIME);
    snprintf(text, sizeof(text), "%.1f Hz", fps); text_at(renderer, right_inner + right_width - 56.0f, content_y + layout.main_height - 28.0f, text, COLOR_MUTED);
    GUI_DrawDropdownOverlay(renderer);
    SDL_RenderPresent(renderer);
}
