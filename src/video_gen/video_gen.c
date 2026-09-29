#include "video_gen/video_gen.h"
#include <math.h>
#include <stdint.h>

static uint32_t rng_state = 0xA341316Cu;

static float random_unit(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return (float)(rng_state >> 8) / 16777216.0f;
}

static float random_signed(void)
{
    return random_unit() * 2.0f - 1.0f;
}

static float clamp_float(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}

void VideoGen_UpdateTarget(const SimConfig_t *config, SimState_t *state, float delta_seconds)
{
    state->simulation_time += delta_seconds;
    if (config->manual_target) return;
    if (config->motion_pattern == MOTION_CIRCULAR) {
        const float angle = state->simulation_time * config->target_speed * 0.003f;
        state->true_target_x = 1000.0f + cosf(angle) * 650.0f;
        state->true_target_y = 1000.0f + sinf(angle) * 650.0f;
    } else if (config->motion_pattern == MOTION_RANDOM) {
        state->true_target_x += random_signed() * config->target_speed * delta_seconds * 2.5f;
        state->true_target_y += random_signed() * config->target_speed * delta_seconds * 2.5f;
    } else {
        state->true_target_x += config->target_speed * delta_seconds * 2.2f;
        state->true_target_y = 1000.0f + sinf(state->simulation_time * 0.7f) * 360.0f;
    }
    const float jitter_step = config->platform_jitter * delta_seconds * 60.0f;
    state->true_target_x += random_signed() * jitter_step;
    state->true_target_y += random_signed() * jitter_step;
    state->true_target_x = clamp_float(state->true_target_x, 30.0f, WORLD_SIZE - 30.0f);
    state->true_target_y = clamp_float(state->true_target_y, 30.0f, WORLD_SIZE - 30.0f);
}

static void draw_beacon(const SimConfig_t *config, FrameBuffer_t *frame, int center_x, int center_y)
{
    if (config->target_shape == TARGET_WIDE) {
        for (int y = center_y - 4; y <= center_y + 4; ++y) {
            for (int x = center_x - 10; x <= center_x + 10; ++x) {
                if (x >= 0 && x < FRAME_WIDTH && y >= 0 && y < FRAME_HEIGHT) frame->pixels[y * FRAME_WIDTH + x] = 255;
            }
        }
        return;
    }
    for (int y = center_y - 5; y <= center_y + 5; ++y) {
        for (int x = center_x - 5; x <= center_x + 5; ++x) {
            if (x < 0 || x >= FRAME_WIDTH || y < 0 || y >= FRAME_HEIGHT) continue;
            if (config->target_shape == TARGET_CIRCLE && (x - center_x) * (x - center_x) + (y - center_y) * (y - center_y) > 30) continue;
            frame->pixels[y * FRAME_WIDTH + x] = 255;
        }
    }
}

void VideoGen_MoveTarget(SimState_t *state, float delta_x, float delta_y)
{
    state->true_target_x = clamp_float(state->true_target_x + delta_x, 30.0f, WORLD_SIZE - 30.0f);
    state->true_target_y = clamp_float(state->true_target_y + delta_y, 30.0f, WORLD_SIZE - 30.0f);
}

void VideoGen_Generate(const SimConfig_t *config, const SimState_t *state, FrameBuffer_t *frame)
{
    const float view_width = 640.0f;
    const float view_height = 480.0f;
    const float left = clamp_float(state->camera_pan - view_width * 0.5f, 0.0f, WORLD_SIZE - view_width);
    const float top = clamp_float(state->camera_tilt - view_height * 0.5f, 0.0f, WORLD_SIZE - view_height);
    frame->width = FRAME_WIDTH;
    frame->height = FRAME_HEIGHT;

    for (int y = 0; y < FRAME_HEIGHT; ++y) {
        for (int x = 0; x < FRAME_WIDTH; ++x) {
            const float texture = 18.0f + 7.0f * sinf((left + x) * 0.025f) * cosf((top + y) * 0.021f);
            const float noise = config->gaussian_noise ? random_signed() * config->noise_intensity * 0.55f : 0.0f;
            float value = texture + noise;
            value = value * (1.0f - config->haze_level * 0.75f) + 42.0f * config->haze_level;
            frame->pixels[y * FRAME_WIDTH + x] = (uint8_t)clamp_float(value, 0.0f, 255.0f);
        }
    }

    draw_beacon(config, frame, (int)(state->true_target_x - left), (int)(state->true_target_y - top));
    if (config->target_mode == TARGET_MULTI) {
        draw_beacon(config, frame, (int)(WORLD_SIZE - state->true_target_x - left), (int)(WORLD_SIZE - state->true_target_y - top));
    }

    if (config->salt_pepper_noise) {
        const int spike_count = (int)(config->noise_intensity * 8.0f);
        for (int i = 0; i < spike_count; ++i) {
            const int x = (int)(random_unit() * FRAME_WIDTH);
            const int y = (int)(random_unit() * FRAME_HEIGHT);
            frame->pixels[y * FRAME_WIDTH + x] = random_unit() > 0.5f ? 255 : 0;
        }
    }
    if (config->poisson_noise) {
        for (int i = 0; i < FRAME_PIXELS; ++i) {
            const float value = frame->pixels[i];
            frame->pixels[i] = (uint8_t)clamp_float(value + random_signed() * sqrtf(value + 1.0f) * 2.0f, 0.0f, 255.0f);
        }
    }
}
