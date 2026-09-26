#include "cv_tracker.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void beacon(FrameBuffer_t *frame, int x, int y)
{
    for (int row = y - 5; row <= y + 5; ++row) {
        for (int column = x - 5; column <= x + 5; ++column) {
            if (column >= 0 && column < frame->width && row >= 0 && row < frame->height) {
                frame->pixels[row * frame->width + column] = 255;
            }
        }
    }
}

int main(void)
{
    const SimConfig_t config = { 70.0f, 5.0f, 5.0f, 0.0f, 0.0f, 0.025f, 0.0002f, 0.006f, false, false, false, MOTION_LINEAR };
    FrameBuffer_t frame = { { 0 }, FRAME_WIDTH, FRAME_HEIGHT };
    CVTracker_Reset();

    beacon(&frame, 320, 240);
    CVResult_t result = CVTracker_Process(&config, &frame, 0.016f, 0.0f);
    assert(result.detected);

    memset(frame.pixels, 0, sizeof(frame.pixels));
    beacon(&frame, 350, 240);
    result = CVTracker_Process(&config, &frame, 0.016f, 0.0f);
    assert(result.detected && result.pan_adjustment > 0.0f);

    memset(frame.pixels, 0, sizeof(frame.pixels));
    beacon(&frame, 300, 240);
    result = CVTracker_Process(&config, &frame, 0.016f, 0.0f);
    assert(result.detected && result.preferred_search_direction < 0);

    CVTracker_Reset();
    memset(frame.pixels, 0, sizeof(frame.pixels));
    beacon(&frame, 320, 180);
    result = CVTracker_Process(&config, &frame, 0.016f, 0.0f);
    assert(result.detected && result.preferred_search_vertical_direction < 0);

    CVTracker_Reset();
    memset(frame.pixels, 0, sizeof(frame.pixels));
    beacon(&frame, 320, 240);
    result = CVTracker_Process(&config, &frame, 0.016f, 0.0f);
    assert(result.detected);
    memset(frame.pixels, 0, sizeof(frame.pixels));
    beacon(&frame, 350, 240);
    result = CVTracker_Process(&config, &frame, 0.016f, 0.0f);
    assert(result.detected && result.pan_adjustment > 0.0f);

    memset(frame.pixels, 0, sizeof(frame.pixels));
    for (int noise = 0; noise < 20; ++noise) {
        const int noise_x = 10 + noise * 13;
        const int noise_y = 20 + noise * 11;
        frame.pixels[noise_y * frame.width + noise_x] = 255;
    }
    for (int frame_number = 0; frame_number < 8; ++frame_number) {
        result = CVTracker_Process(&config, &frame, 0.016f, 0.1f);
        assert(result.using_prediction && result.predicted_x > 320.0f);
        assert(result.pan_adjustment > 0.0f);
    }
    assert(result.searching);

    CVTracker_SetExternalPrediction(360.0f, 240.0f, 0.9f);
    beacon(&frame, 360, 240);
    result = CVTracker_Process(&config, &frame, 0.016f, 0.1f);
    assert(result.detected);
    puts("tracker loss, directional prediction, and reacquisition ok");
    return 0;
}
