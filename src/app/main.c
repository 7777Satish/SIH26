#include <SDL3/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "types/types.h"
#include "profiler/profiler.h"
#include "video_gen/video_gen.h"
#include "cv_tracker/cv_tracker.h"
#include "renderer/renderer.h"
#include "predictor_bridge/predictor_bridge.h"

static float previous_frame_seconds;
static bool search_active;
static int search_pan_direction;
static int search_tilt_direction;
static float search_tilt_target;

static float clamp_world(float value, float low, float high)
{
	return value < low ? low : (value > high ? high : value);
}

static void ApplySearchScan(const SimConfig_t *config, SimState_t *state, const CVResult_t *cv)
{
	const float min_pan = 320.0f;
	const float max_pan = WORLD_SIZE - 320.0f;
	const float min_tilt = 240.0f;
	const float max_tilt = WORLD_SIZE - 240.0f;
	if (!cv->searching) {
		search_active = false;
		return;
	}
	if (!search_active) {
		search_active = true;
		search_pan_direction = cv->preferred_search_direction != 0 ? cv->preferred_search_direction
			: (state->camera_pan >= WORLD_SIZE * 0.5f ? -1 : 1);
		search_tilt_direction = cv->preferred_search_vertical_direction;
		search_tilt_target = search_tilt_direction == 0 ? state->camera_tilt
			: clamp_world(state->camera_tilt + search_tilt_direction * FRAME_HEIGHT, min_tilt, max_tilt);
	}
	if ((search_pan_direction > 0 && state->camera_pan >= max_pan - 2.0f) ||
		(search_pan_direction < 0 && state->camera_pan <= min_pan + 2.0f)) {
		search_pan_direction = -search_pan_direction;
		if (search_tilt_direction == 0) search_tilt_direction = state->camera_tilt >= WORLD_SIZE * 0.5f ? -1 : 1;
		search_tilt_target = clamp_world(state->camera_tilt + search_tilt_direction * FRAME_HEIGHT, min_tilt, max_tilt);
		if (search_tilt_target == min_tilt || search_tilt_target == max_tilt) search_tilt_direction = -search_tilt_direction;
	}
	state->pan_velocity = search_pan_direction * config->max_pan_speed;
	state->tilt_velocity = fabsf(state->camera_tilt - search_tilt_target) > 2.0f
		? search_tilt_direction * config->max_tilt_speed : 0.0f;
}

int main(void)
{
	CVTracker_Reset();
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window *window = SDL_CreateWindow("Orbital Eye - Virtual Camera Tracking", 1490, 820, SDL_WINDOW_RESIZABLE);
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
	SDL_SetRenderLogicalPresentation(renderer, 1490, 820, SDL_LOGICAL_PRESENTATION_LETTERBOX);

	SimConfig_t config = { 70.0f, 5.0f, 5.0f, 18.0f, 0.12f, 0.025f, 0.0002f, 0.006f, true, true, false, MOTION_CIRCULAR };
	SimState_t state = { 1650.0f, 1000.0f, 1650.0f, 1000.0f, 0.0f, 0.0f, 0.0f };
	FrameBuffer_t frame = { { 0 }, FRAME_WIDTH, FRAME_HEIGHT };
	CVResult_t cv = { 0 };
	ProfilerState_t profiler = { 0 };
	PredictorBridge_t predictor = { 0 };
	PredictorBridge_Init(&predictor);
	bool running = true;
	uint64_t previous_ticks = SDL_GetTicksNS();
	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) running = false;
			if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
			if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F11) {
				const bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0;
				SDL_SetWindowFullscreen(window, fullscreen);
			}
		}
		const uint64_t now = SDL_GetTicksNS();
		float delta_seconds = (float)(now - previous_ticks) / 1000000000.0f;
		previous_ticks = now;
		if (delta_seconds > 0.1f) delta_seconds = 0.1f;
		if (config.manual_target) {
			const bool *keys = SDL_GetKeyboardState(NULL);
			const float manual_speed = 420.0f * delta_seconds;
			float move_x = 0.0f;
			float move_y = 0.0f;
			if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) move_x -= manual_speed;
			if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) move_x += manual_speed;
			if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) move_y -= manual_speed;
			if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) move_y += manual_speed;
			VideoGen_MoveTarget(&state, move_x, move_y);
		}

		Profiler_BeginFrame(&profiler);
		PROFILE_START(&profiler, PROFILER_VIDEO);
		VideoGen_UpdateTarget(&config, &state, delta_seconds);
		VideoGen_Generate(&config, &state, &frame);
		PROFILE_END(&profiler, PROFILER_VIDEO);
		PROFILE_START(&profiler, PROFILER_CV);
		float predicted_x;
		float predicted_y;
		float prediction_confidence;
		if (PredictorBridge_Poll(&predictor, &predicted_x, &predicted_y, &prediction_confidence)) {
			CVTracker_SetExternalPrediction(predicted_x, predicted_y, prediction_confidence);
		}
		cv = CVTracker_Process(&config, &frame, delta_seconds, previous_frame_seconds);
		PROFILE_END(&profiler, PROFILER_CV);
		PredictorBridge_Submit(&predictor, cv.detected ? cv.centroid_x : cv.predicted_x, cv.detected ? cv.centroid_y : cv.predicted_y, delta_seconds, previous_frame_seconds);
		state.pan_velocity = cv.pan_adjustment;
		state.tilt_velocity = cv.tilt_adjustment;
		ApplySearchScan(&config, &state, &cv);
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
		previous_frame_seconds = (float)profiler.frame_ns / 1000000000.0f;
	}
	PredictorBridge_Shutdown(&predictor);
	Renderer_DestroyFeedTexture(feed_texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
