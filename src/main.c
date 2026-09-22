#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdio.h>

#include "types.h"
#include "profiler.h"
#include "video_gen.h"
#include "cv_tracker.h"
#include "renderer.h"

int main(void)
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window *window = SDL_CreateWindow("Orbital Eye - Virtual Camera Tracking", 1490, 820, 0);
	SDL_Renderer *renderer = window != NULL ? SDL_CreateRenderer(window, NULL) : NULL;
	SDL_Texture *feed_texture = renderer != NULL ? Renderer_CreateFeedTexture(renderer) : NULL;
	if (window == NULL || renderer == NULL || feed_texture == NULL) {
		fprintf(stderr, "SDL resource creation failed: %s\n", SDL_GetError());
		if (feed_texture != NULL) Renderer_DestroyFeedTexture(feed_texture);
		if (renderer != NULL) SDL_DestroyRenderer(renderer);
		if (window != NULL) SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	SimConfig_t config = { 70.0f, 5.0f, 5.0f, 18.0f, 0.12f, 0.025f, 0.0002f, 0.006f, true, true, MOTION_CIRCULAR };
	SimState_t state = { 1650.0f, 1000.0f, 1000.0f, 1000.0f, 0.0f, 0.0f, 0.0f };
	FrameBuffer_t frame = { { 0 }, FRAME_WIDTH, FRAME_HEIGHT };
	CVResult_t cv = { 0 };
	ProfilerState_t profiler = { 0 };
	bool running = true;
	uint64_t previous_ticks = SDL_GetTicksNS();
	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) running = false;
			if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
		}
		const uint64_t now = SDL_GetTicksNS();
		float delta_seconds = (float)(now - previous_ticks) / 1000000000.0f;
		previous_ticks = now;
		if (delta_seconds > 0.1f) delta_seconds = 0.1f;

		Profiler_BeginFrame(&profiler);
		PROFILE_START(&profiler, PROFILER_VIDEO);
		VideoGen_UpdateTarget(&config, &state, delta_seconds);
		VideoGen_Generate(&config, &state, &frame);
		PROFILE_END(&profiler, PROFILER_VIDEO);
		PROFILE_START(&profiler, PROFILER_CV);
		cv = CVTracker_Process(&config, &frame, delta_seconds);
		PROFILE_END(&profiler, PROFILER_CV);
		state.pan_velocity = cv.pan_adjustment;
		state.tilt_velocity = cv.tilt_adjustment;
		state.camera_pan += state.pan_velocity * delta_seconds * 35.0f;
		state.camera_tilt += state.tilt_velocity * delta_seconds * 35.0f;
		if (state.camera_pan < 320.0f) state.camera_pan = 320.0f;
		if (state.camera_pan > WORLD_SIZE - 320.0f) state.camera_pan = WORLD_SIZE - 320.0f;
		if (state.camera_tilt < 240.0f) state.camera_tilt = 240.0f;
		if (state.camera_tilt > WORLD_SIZE - 240.0f) state.camera_tilt = WORLD_SIZE - 240.0f;
		PROFILE_START(&profiler, PROFILER_RENDERING);
		PROFILE_START(&profiler, PROFILER_UI);
		const float fps = delta_seconds > 0.0f ? 1.0f / delta_seconds : 0.0f;
		Renderer_DrawDashboard(renderer, feed_texture, &frame, &config, &state, &cv, &profiler, fps);
		PROFILE_END(&profiler, PROFILER_UI);
		PROFILE_END(&profiler, PROFILER_RENDERING);
		Profiler_EndFrame(&profiler);
	}
	Renderer_DestroyFeedTexture(feed_texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
