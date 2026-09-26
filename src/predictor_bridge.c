#include "predictor_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32

bool PredictorBridge_Init(PredictorBridge_t *bridge)
{
    *bridge = (PredictorBridge_t){ 0 };
    return false;
}

void PredictorBridge_Shutdown(PredictorBridge_t *bridge)
{
    (void)bridge;
}

void PredictorBridge_Submit(PredictorBridge_t *bridge, float x, float y, float delta_seconds, float latency_seconds)
{
    (void)bridge;
    (void)x;
    (void)y;
    (void)delta_seconds;
    (void)latency_seconds;
}

bool PredictorBridge_Poll(PredictorBridge_t *bridge, float *x, float *y, float *confidence)
{
    (void)bridge;
    (void)x;
    (void)y;
    (void)confidence;
    return false;
}

#else

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

static const int predictor_port = 47001;

bool PredictorBridge_Init(PredictorBridge_t *bridge)
{
    *bridge = (PredictorBridge_t){ .socket_fd = -1 };
    const char *enabled = getenv("ORBITAL_PREDICTOR");
    if (enabled == NULL || strcmp(enabled, "1") != 0) return false;
    bridge->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (bridge->socket_fd < 0) return false;
    const int flags = fcntl(bridge->socket_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(bridge->socket_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(bridge->socket_fd);
        bridge->socket_fd = -1;
        return false;
    }
    bridge->enabled = true;
    return true;
}

void PredictorBridge_Shutdown(PredictorBridge_t *bridge)
{
    if (bridge->socket_fd >= 0) close(bridge->socket_fd);
    bridge->socket_fd = -1;
    bridge->enabled = false;
}

void PredictorBridge_Submit(PredictorBridge_t *bridge, float x, float y, float delta_seconds, float latency_seconds)
{
    if (!bridge->enabled) return;
    char message[192];
    const int length = snprintf(message, sizeof(message),
        "{\"sequence\":%u,\"x\":%.3f,\"y\":%.3f,\"dt\":%.6f,\"latency\":%.6f}",
        bridge->sequence++, x, y, delta_seconds, latency_seconds);
    struct sockaddr_in address = { 0 };
    address.sin_family = AF_INET;
    address.sin_port = htons(predictor_port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    (void)sendto(bridge->socket_fd, message, (size_t)length, 0, (struct sockaddr *)&address, sizeof(address));
}

bool PredictorBridge_Poll(PredictorBridge_t *bridge, float *x, float *y, float *confidence)
{
    if (!bridge->enabled) return false;
    char message[256];
    bool received = false;
    for (;;) {
        const ssize_t length = recv(bridge->socket_fd, message, sizeof(message) - 1, 0);
        if (length < 0) break;
        message[length] = '\0';
        float next_x;
        float next_y;
        float next_confidence;
        if (sscanf(message, "{\"x\":%f,\"y\":%f,\"confidence\":%f}", &next_x, &next_y, &next_confidence) == 3) {
            *x = next_x;
            *y = next_y;
            *confidence = next_confidence;
            received = true;
        }
    }
    return received;
}

#endif