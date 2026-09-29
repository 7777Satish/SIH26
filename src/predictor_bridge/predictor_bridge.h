#ifndef PREDICTOR_BRIDGE_H
#define PREDICTOR_BRIDGE_H

#include <stdbool.h>

typedef struct PredictorBridge {
    int socket_fd;
    bool enabled;
    unsigned int sequence;
} PredictorBridge_t;

bool PredictorBridge_Init(PredictorBridge_t *bridge);
void PredictorBridge_Shutdown(PredictorBridge_t *bridge);
void PredictorBridge_Submit(PredictorBridge_t *bridge, float x, float y, float delta_seconds, float latency_seconds);
bool PredictorBridge_Poll(PredictorBridge_t *bridge, float *x, float *y, float *confidence);

#endif