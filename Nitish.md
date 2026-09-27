# Orbital Eye / Virtual Camera Tracking

## Session Implementation Record

This document records the work completed during the session on the SIH26 virtual-camera tracking project.

The project is a C11 SDL3 simulation of a virtual camera tracking a bright target inside a 2000 x 2000 world. The camera renders a 640 x 480 grayscale feed, detects the target, estimates motion, controls simulated pan/tilt movement, and displays the complete state in an SDL dashboard.

## Existing Architecture

- `src/main.c`: SDL startup, event loop, simulation loop, camera actuation, predictor bridge integration, and fullscreen handling.
- `src/video_gen.c/.h`: target motion, manual target movement, synthetic camera-frame generation, Gaussian noise, haze, and salt-and-pepper noise.
- `src/cv_tracker.c/.h`: bright-target detection, connected-component filtering, motion estimation, prediction, adaptive ROI, PID control, confidence, and lost-target state.
- `src/types.h`: shared configuration, simulation state, frame buffer, tracker result, and profiler data structures.
- `src/renderer.c/.h`: dashboard layout, global-world map, camera feed, tracking overlays, telemetry, and visual status.
- `src/gui.c/.h`: lightweight SDL controls, sliders, toggles, buttons, active mode buttons, and logical mouse-coordinate handling.
- `src/profiler.c/.h`: frame and pipeline timing measurements.
- `src/predictor_bridge.c/.h`: optional non-blocking C/Python UDP interface.
- `predictor.py`: optional asynchronous constant-velocity or TorchScript motion predictor.
- `tests/test_cv_tracker.c`: focused regression test for detection, target loss, prediction, noise rejection, direction selection, and reacquisition.
- `Makefile`: Linux SDL3 build and clean commands.

## Tracking and Prediction Work

### Connected-component target detection

The original tracker averaged every pixel above the brightness threshold. That caused salt-and-pepper white noise to become a false target centroid and corrupted velocity prediction.

The tracker now:

1. Searches an adaptive region of interest around the predicted target position.
2. Finds connected bright-pixel components using a small flood-fill queue.
3. Selects the largest connected component.
4. Rejects components smaller than nine pixels.
5. Treats the 11 x 11 beacon as the valid target while ignoring isolated white noise.

The frame remains a grayscale image with a brightness threshold of 200.

### Motion estimation

`CVTracker_Process` maintains a persistent tracker state containing:

- Position `x/y`.
- Velocity `vx/vy`.
- Position and velocity covariance values.
- PID integral and previous-error state.
- Adaptive ROI radius.
- Frames since last detection.
- Preferred horizontal and vertical search directions.

The estimator predicts the state between measurements, then updates position and velocity from the newest valid beacon observation. Velocity is smoothed using the new measurement displacement so a single noisy observation does not reverse the camera direction.

The PID controller uses the latency-compensated predicted position, clamps integral windup, and respects configured maximum pan and tilt speeds.

### Latency compensation

The previous frame duration is used as an approximate actuation/rendering latency horizon. The tracker projects the current estimated position forward before calculating control error.

Predicted coordinates are intentionally allowed to move outside the 640 x 480 image. Clamping them to the frame boundary previously destroyed the direction of motion when the target left the feed.

### External prediction

The C tracker accepts an external prediction only when confidence is at least `0.55`. An accepted external prediction:

- Centers the next ROI search.
- Replaces the short-horizon C prediction for control.
- Is consumed once so stale predictions are not reused indefinitely.

The C estimator remains the fallback when Python is disabled, unavailable, or below the confidence threshold.

## Observation-Centric Search Direction

The session reviewed the following research direction:

- **Observation-Centric SORT (OC-SORT), CVPR 2023**, arXiv:2203.14360.
- **Simple Online and Realtime Tracking (SORT)**, arXiv:1602.00763.

The useful principle applied here is to preserve reliable observation trajectory information during missing detections instead of allowing an open-loop filter to drift freely during occlusion.

The tracker now records a two-dimensional exit vector:

- Left or right based on the last target position and horizontal velocity.
- Up or down based on the last target position and vertical velocity.
- Both axes are retained, so diagonal exits produce diagonal search behavior.

This is more appropriate for the simulator than a one-axis last-side flag.

## Full-World Search Sweep

When the target is absent for at least six frames, `CVResult_t.searching` becomes true and the camera enters search mode.

Search behavior:

1. Start in the last reliable horizontal direction.
2. Start in the last reliable vertical direction when available.
3. Move toward the predicted exit quadrant.
4. When a horizontal world boundary is reached, reverse pan direction.
5. Step vertically by approximately one camera field height.
6. Continue as a serpentine scan through the 2000 x 2000 world.
7. Stop the search immediately when a valid target detection returns.

The dashboard displays `SEARCH SWEEP ACTIVE` during this phase.

## Manual Target Testing

Manual target mode was added to make prediction and reacquisition reproducible.

When `MANUAL TARGET` is enabled:

- Automatic linear, circular, or random target motion pauses.
- Hold arrow keys or `WASD` to move the target.
- Movement is frame-rate independent.
- Target movement is clamped inside the world boundaries.

Recommended manual test:

1. Enable `MANUAL TARGET`.
2. Move the beacon near the center and verify `TARGET LOCKED`.
3. Move it out of the camera frame in a chosen direction.
4. Observe `PREDICTIVE HOLD` during the short prediction period.
5. Observe `SEARCH SWEEP ACTIVE` after missed detections.
6. Move or wait for the scan to bring the camera across the target.
7. Verify the target reticle and `TARGET LOCKED` status return.

## UI and Fullscreen Work

The dashboard was improved in several areas:

- Active motion-mode buttons now have a distinct selected state.
- Camera feed uses an explicit RGBA streaming texture instead of an unconfigured indexed texture.
- Nearest-neighbor scaling keeps the synthetic feed crisp.
- Global map has grid lines for spatial orientation.
- Target has a visible amber reticle on the global map.
- Camera feed shows a cyan measured-target reticle.
- Camera feed shows an amber predicted-target reticle when the target is not currently detected but prediction remains available.
- Dashboard displays confidence and ROI radius.
- Status changes between `TARGET LOCKED`, `PREDICTIVE HOLD`, and `SEARCH SWEEP ACTIVE`.
- Window is resizable.
- SDL logical presentation keeps the 1490 x 820 layout aligned in larger windows and fullscreen.
- Mouse coordinates are converted from physical window coordinates to logical dashboard coordinates, keeping sliders and buttons clickable after scaling.
- `F11` toggles fullscreen.
- `Escape` exits.

## Optional Python Predictor

The Python predictor is intentionally optional. The C simulation does not depend on it.

The bridge uses non-blocking UDP on `127.0.0.1:47001`:

1. C sends the newest observation, time step, and latency.
2. Python drains queued packets and keeps only the latest observation.
3. Python returns predicted `x`, `y`, and confidence.
4. C accepts the prediction only when confidence is at least `0.55`.

Without Torch, `predictor.py` uses a deterministic constant-velocity fallback. With Torch and a TorchScript model, it can load the model using `--model`.

Run it from WSL:

```sh
cd /mnt/d/Claude/SIH/SIH26
python3 predictor.py
```

In another WSL terminal:

```sh
cd /mnt/d/Claude/SIH/SIH26
ORBITAL_PREDICTOR=1 ./virtual_camera
```

The default C tracker works without starting Python.

## Build and Run

The project targets Linux and uses SDL3, GNU Make, GCC or Clang, `pkg-config`, and `libm`.

From Windows, the built executable is a Linux ELF binary and must be launched through WSL. WSLg is required for the SDL window:

```powershell
wsl --cd /mnt/d/Claude/SIH/SIH26 make
wsl --cd /mnt/d/Claude/SIH/SIH26 ./virtual_camera
```

Clean and rebuild:

```powershell
wsl --cd /mnt/d/Claude/SIH/SIH26 make clean
wsl --cd /mnt/d/Claude/SIH/SIH26 make
```

The build uses:

```text
-std=c11 -Wall -Wextra -Wpedantic -O2
```

## Validation Completed

The following checks passed during the session:

- Production SDL3 build with strict compiler warnings.
- Python syntax compilation with `python3 -m py_compile predictor.py`.
- Python predictor fallback behavior test.
- C tracker regression test.
- Connected-component noise rejection test.
- Prediction during target loss.
- Left-side exit-direction test.
- Vertical exit-direction test.
- External prediction-guided reacquisition test.
- Full-world search activation test.
- Manual-control integration build.
- Resizable/fullscreen UI build.
- Editor diagnostics on modified C and Python files.
- `git diff --check`.

## Current Limitations

- The main production executable is Linux-only unless a native Windows SDL3 build is added.
- SDL window behavior cannot be fully screenshot-tested from the current headless WSL environment.
- The Python TorchScript path requires a compatible model input/output shape; the built-in fallback is the default safe path.
- A target moving faster than the configured physical camera limits cannot be tracked indefinitely by any predictor; prediction can bridge temporary loss, but it cannot overcome actuator limits permanently.
