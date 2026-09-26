#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define WORLD_SIZE 2000.0f
#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480
#define FRAME_PIXELS (FRAME_WIDTH * FRAME_HEIGHT)
#define PROFILER_LAYER_COUNT 4

typedef enum MotionPattern {
    MOTION_LINEAR = 0,
    MOTION_CIRCULAR,
    MOTION_RANDOM
} MotionPattern_t;

typedef struct SimConfig {
    float target_speed;
    float max_pan_speed;
    float max_tilt_speed;
    float noise_intensity;
    float haze_level;
    float kp;
    float ki;
    float kd;
    bool gaussian_noise;
    bool salt_pepper_noise;
    bool manual_target;
    MotionPattern_t motion_pattern;
} SimConfig_t;

typedef struct SimState {
    float true_target_x;
    float true_target_y;
    float camera_pan;
    float camera_tilt;
    float pan_velocity;
    float tilt_velocity;
    float simulation_time;
} SimState_t;

typedef struct FrameBuffer {
    uint8_t pixels[FRAME_PIXELS];
    int width;
    int height;
} FrameBuffer_t;

typedef struct CVResult {
    float centroid_x;
    float centroid_y;
    float predicted_x;
    float predicted_y;
    float error_x;
    float error_y;
    float pixel_error;
    float pan_adjustment;
    float tilt_adjustment;
    float confidence;
    float roi_radius;
    int bright_pixels;
    int frames_since_detection;
    int preferred_search_direction;
    int preferred_search_vertical_direction;
    bool detected;
    bool searching;
    bool using_prediction;
} CVResult_t;

typedef struct ProfilerState {
    uint64_t start_ns[PROFILER_LAYER_COUNT];
    uint64_t rolling_ns[PROFILER_LAYER_COUNT];
    float milliseconds[PROFILER_LAYER_COUNT];
    float cpu_percent[PROFILER_LAYER_COUNT];
    uint64_t frame_ns;
} ProfilerState_t;

#endif
