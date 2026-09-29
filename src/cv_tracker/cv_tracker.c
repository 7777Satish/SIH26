#include "cv_tracker/cv_tracker.h"
#include <math.h>
#include <string.h>

typedef struct TrackerState {
    float x;
    float y;
    float velocity_x;
    float velocity_y;
    float covariance[4][4];
    float integral_x;
    float integral_y;
    float previous_error_x;
    float previous_error_y;
    float roi_radius;
    int frames_since_detection;
    int preferred_search_direction;
    int preferred_search_vertical_direction;
    bool initialized;
} TrackerState_t;

static TrackerState_t tracker;
static float external_prediction_x;
static float external_prediction_y;
static float external_prediction_confidence;
static bool external_prediction_available;
static uint8_t measurement_visited[FRAME_PIXELS];
static int measurement_queue[FRAME_PIXELS];

static float clamp_float(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}

static void predict_state(float delta_seconds)
{
    tracker.x += tracker.velocity_x * delta_seconds;
    tracker.y += tracker.velocity_y * delta_seconds;
    tracker.covariance[0][0] += delta_seconds * delta_seconds * 80.0f;
    tracker.covariance[1][1] += delta_seconds * delta_seconds * 80.0f;
    tracker.covariance[2][2] += delta_seconds * 12.0f;
    tracker.covariance[3][3] += delta_seconds * 12.0f;
}

static void update_axis(float *position, float *velocity, float *position_variance, float *velocity_variance, float measurement, float delta_seconds)
{
    const float measurement_variance = 16.0f;
    const float innovation_variance = *position_variance + measurement_variance;
    const float position_gain = *position_variance / innovation_variance;
    const float innovation = measurement - *position;
    const float measured_velocity = innovation / delta_seconds;
    *position += position_gain * innovation;
    *velocity = *velocity * 0.75f + measured_velocity * 0.25f;
    *position_variance *= 1.0f - position_gain;
    *velocity_variance = clamp_float(*velocity_variance + 12.0f * delta_seconds - measured_velocity * measured_velocity * 0.001f, 4.0f, 10000.0f);
}

static bool find_measurement(const FrameBuffer_t *frame, float center_x, float center_y, float radius, float *measurement_x, float *measurement_y, int *bright_pixels)
{
    const int left = (int)clamp_float(center_x - radius, 0.0f, (float)(frame->width - 1));
    const int right = (int)clamp_float(center_x + radius, 0.0f, (float)(frame->width - 1));
    const int top = (int)clamp_float(center_y - radius, 0.0f, (float)(frame->height - 1));
    const int bottom = (int)clamp_float(center_y + radius, 0.0f, (float)(frame->height - 1));
    double best_sum_x = 0.0;
    double best_sum_y = 0.0;
    int best_count = 0;
    memset(measurement_visited, 0, sizeof(measurement_visited));
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const int start = y * frame->width + x;
            if (measurement_visited[start] || frame->pixels[start] < 200) continue;
            int queue_head = 0;
            int queue_tail = 0;
            double sum_x = 0.0;
            double sum_y = 0.0;
            measurement_queue[queue_tail++] = start;
            measurement_visited[start] = 1;
            while (queue_head < queue_tail) {
                const int current = measurement_queue[queue_head++];
                const int current_x = current % frame->width;
                const int current_y = current / frame->width;
                sum_x += current_x;
                sum_y += current_y;
                for (int offset_y = -1; offset_y <= 1; ++offset_y) {
                    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
                        const int neighbor_x = current_x + offset_x;
                        const int neighbor_y = current_y + offset_y;
                        if (neighbor_x < left || neighbor_x > right || neighbor_y < top || neighbor_y > bottom) continue;
                        const int neighbor = neighbor_y * frame->width + neighbor_x;
                        if (!measurement_visited[neighbor] && frame->pixels[neighbor] >= 200) {
                            measurement_visited[neighbor] = 1;
                            measurement_queue[queue_tail++] = neighbor;
                        }
                    }
                }
            }
            if (queue_tail > best_count) {
                best_count = queue_tail;
                best_sum_x = sum_x;
                best_sum_y = sum_y;
            }
        }
    }
    if (best_count < 9) return false;
    *measurement_x = (float)(best_sum_x / best_count);
    *measurement_y = (float)(best_sum_y / best_count);
    *bright_pixels = best_count;
    return true;
}

static bool find_global_measurement(const FrameBuffer_t *frame, float *measurement_x, float *measurement_y, int *bright_pixels)
{
    return find_measurement(frame, frame->width * 0.5f, frame->height * 0.5f, 1000.0f, measurement_x, measurement_y, bright_pixels);
}

void CVTracker_Reset(void)
{
    tracker = (TrackerState_t){ 0 };
    tracker.roi_radius = 72.0f;
    tracker.preferred_search_direction = 0;
    tracker.preferred_search_vertical_direction = 0;
    external_prediction_available = false;
}

void CVTracker_SetExternalPrediction(float x, float y, float confidence)
{
    if (confidence >= 0.55f) {
        external_prediction_x = x;
        external_prediction_y = y;
        external_prediction_confidence = confidence;
        external_prediction_available = true;
    }
}

CVResult_t CVTracker_Process(const SimConfig_t *config, const FrameBuffer_t *frame, float delta_seconds, float latency_seconds)
{
    CVResult_t result = { 0 };
    const float safe_delta = delta_seconds > 0.0f ? delta_seconds : 0.001f;
    if (!tracker.initialized) {
        tracker.x = frame->width * 0.5f;
        tracker.y = frame->height * 0.5f;
        tracker.covariance[0][0] = 400.0f;
        tracker.covariance[1][1] = 400.0f;
        tracker.covariance[2][2] = 100.0f;
        tracker.covariance[3][3] = 100.0f;
        tracker.initialized = true;
    }

    predict_state(safe_delta);
    float measurement_x = 0.0f;
    float measurement_y = 0.0f;
    const bool has_external_prediction = external_prediction_available && external_prediction_confidence >= 0.55f;
    const float search_x = has_external_prediction ? external_prediction_x : tracker.x;
    const float search_y = has_external_prediction ? external_prediction_y : tracker.y;
    bool detected = find_measurement(frame, search_x, search_y, tracker.roi_radius, &measurement_x, &measurement_y, &result.bright_pixels);
    if (!detected && tracker.frames_since_detection >= 3) {
        detected = find_global_measurement(frame, &measurement_x, &measurement_y, &result.bright_pixels);
    }
    if (detected) {
        result.detected = true;
        result.centroid_x = measurement_x;
        result.centroid_y = measurement_y;
        update_axis(&tracker.x, &tracker.velocity_x, &tracker.covariance[0][0], &tracker.covariance[2][2], measurement_x, safe_delta);
        update_axis(&tracker.y, &tracker.velocity_y, &tracker.covariance[1][1], &tracker.covariance[3][3], measurement_y, safe_delta);
        if (measurement_x < FRAME_WIDTH * 0.4f) tracker.preferred_search_direction = -1;
        else if (measurement_x > FRAME_WIDTH * 0.6f) tracker.preferred_search_direction = 1;
        else if (fabsf(tracker.velocity_x) > 8.0f) tracker.preferred_search_direction = tracker.velocity_x < 0.0f ? -1 : 1;
        if (measurement_y < FRAME_HEIGHT * 0.4f) tracker.preferred_search_vertical_direction = -1;
        else if (measurement_y > FRAME_HEIGHT * 0.6f) tracker.preferred_search_vertical_direction = 1;
        else if (fabsf(tracker.velocity_y) > 8.0f) tracker.preferred_search_vertical_direction = tracker.velocity_y < 0.0f ? -1 : 1;
        tracker.frames_since_detection = 0;
        tracker.roi_radius = clamp_float(tracker.roi_radius * 0.85f, 48.0f, 180.0f);
    } else {
        ++tracker.frames_since_detection;
        tracker.roi_radius = clamp_float(tracker.roi_radius * 1.35f, 48.0f, 320.0f);
        result.using_prediction = true;
    }

    const float horizon = clamp_float(latency_seconds, 0.0f, 0.25f);
    result.predicted_x = tracker.x + tracker.velocity_x * horizon;
    result.predicted_y = tracker.y + tracker.velocity_y * horizon;
    if (has_external_prediction) {
        result.predicted_x = external_prediction_x;
        result.predicted_y = external_prediction_y;
        result.using_prediction = true;
        external_prediction_available = false;
    }
    result.error_x = result.predicted_x - FRAME_WIDTH * 0.5f;
    result.error_y = result.predicted_y - FRAME_HEIGHT * 0.5f;
    result.pixel_error = sqrtf(result.error_x * result.error_x + result.error_y * result.error_y);
    result.confidence = clamp_float(1.0f - tracker.frames_since_detection / 12.0f, 0.0f, 1.0f);
    result.roi_radius = tracker.roi_radius;
    result.frames_since_detection = tracker.frames_since_detection;
    result.preferred_search_direction = tracker.preferred_search_direction;
    result.preferred_search_vertical_direction = tracker.preferred_search_vertical_direction;
    result.searching = tracker.frames_since_detection >= 6;
    {
        tracker.integral_x += result.error_x * safe_delta;
        tracker.integral_y += result.error_y * safe_delta;
        tracker.integral_x = clamp_float(tracker.integral_x, -400.0f, 400.0f);
        tracker.integral_y = clamp_float(tracker.integral_y, -400.0f, 400.0f);
        const float derivative_x = (result.error_x - tracker.previous_error_x) / safe_delta;
        const float derivative_y = (result.error_y - tracker.previous_error_y) / safe_delta;
        result.pan_adjustment = config->kp * result.error_x + config->ki * tracker.integral_x + config->kd * derivative_x;
        result.tilt_adjustment = config->kp * result.error_y + config->ki * tracker.integral_y + config->kd * derivative_y;
        if (result.pan_adjustment > config->max_pan_speed) result.pan_adjustment = config->max_pan_speed;
        if (result.pan_adjustment < -config->max_pan_speed) result.pan_adjustment = -config->max_pan_speed;
        if (result.tilt_adjustment > config->max_tilt_speed) result.tilt_adjustment = config->max_tilt_speed;
        if (result.tilt_adjustment < -config->max_tilt_speed) result.tilt_adjustment = -config->max_tilt_speed;
        tracker.previous_error_x = result.error_x;
        tracker.previous_error_y = result.error_y;
    }
    return result;
}
