#include "cv_tracker.h"
#include <math.h>

static float integral_x;
static float integral_y;
static float previous_x;
static float previous_y;

void CVTracker_Reset(void)
{
    integral_x = 0.0f;
    integral_y = 0.0f;
    previous_x = 0.0f;
    previous_y = 0.0f;
}

CVResult_t CVTracker_Process(const SimConfig_t *config, const FrameBuffer_t *frame, float delta_seconds)
{
    CVResult_t result = { 0 };
    double sum_x = 0.0;
    double sum_y = 0.0;
    for (int y = 0; y < frame->height; ++y) {
        for (int x = 0; x < frame->width; ++x) {
            if (frame->pixels[y * frame->width + x] >= 200) {
                sum_x += x;
                sum_y += y;
                result.bright_pixels++;
            }
        }
    }
    if (result.bright_pixels > 0) {
        result.detected = true;
        result.centroid_x = (float)(sum_x / result.bright_pixels);
        result.centroid_y = (float)(sum_y / result.bright_pixels);
        result.error_x = result.centroid_x - FRAME_WIDTH * 0.5f;
        result.error_y = result.centroid_y - FRAME_HEIGHT * 0.5f;
        result.pixel_error = sqrtf(result.error_x * result.error_x + result.error_y * result.error_y);
        integral_x += result.error_x * delta_seconds;
        integral_y += result.error_y * delta_seconds;
        const float derivative_x = delta_seconds > 0.0f ? (result.error_x - previous_x) / delta_seconds : 0.0f;
        const float derivative_y = delta_seconds > 0.0f ? (result.error_y - previous_y) / delta_seconds : 0.0f;
        result.pan_adjustment = config->kp * result.error_x + config->ki * integral_x + config->kd * derivative_x;
        result.tilt_adjustment = config->kp * result.error_y + config->ki * integral_y + config->kd * derivative_y;
        if (result.pan_adjustment > config->max_pan_speed) result.pan_adjustment = config->max_pan_speed;
        if (result.pan_adjustment < -config->max_pan_speed) result.pan_adjustment = -config->max_pan_speed;
        if (result.tilt_adjustment > config->max_tilt_speed) result.tilt_adjustment = config->max_tilt_speed;
        if (result.tilt_adjustment < -config->max_tilt_speed) result.tilt_adjustment = -config->max_tilt_speed;
        previous_x = result.error_x;
        previous_y = result.error_y;
    }
    return result;
}
