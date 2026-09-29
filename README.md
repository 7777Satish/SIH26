# Orbital Eye / Virtual Camera Tracking

Orbital Eye is a real-time C11 simulation of a virtual camera tracking a bright target in a synthetic video feed. It generates a grayscale 640 x 480 frame, applies configurable signal disturbances, detects bright pixels with a centroid tracker, and uses a PID-style controller to move the simulated pan/tilt camera.

The SDL3 dashboard shows the global 2000 x 2000 simulation space, the active camera field of view, the generated camera feed, controller state, and per-stage timing information.

## Requirements

The project currently targets Linux and requires:

- A C compiler with C11 support, such as GCC or Clang
- GNU Make
- `pkg-config`
- SDL3 development files, including the `sdl3` pkg-config module
- The system math library (`libm`, normally included with the standard C toolchain)
- Python 3.10 or newer for the optional asynchronous predictor
- PyTorch only when loading a TorchScript prediction model; the fallback needs no Python packages

The Makefile asks `pkg-config` for both SDL3 compiler flags and linker flags, so SDL3 must be discoverable before running `make`.

### Ubuntu or Debian

Install the build tools and SDL3 development package:

```sh
sudo apt update
sudo apt install build-essential pkg-config libsdl3-dev
```

If `libsdl3-dev` is not available in your distribution release, install SDL3 from the official SDL release packages or build it from source, then ensure its `sdl3.pc` file is in a directory searched by `pkg-config`.

### Fedora

```sh
sudo dnf install gcc make pkgconf-pkg-config SDL3-devel
```

### Arch Linux

```sh
sudo pacman -S base-devel pkgconf sdl3
```

## Build

From the repository root:

```sh
make
```

This compiles every C source file under the module directories in `src/` with the following default options:

```text
-std=c11 -Wall -Wextra -Wpedantic -O2
```

The resulting executable is created at:

```text
./virtual_camera
```

The C tracker is self-contained by default. To enable the optional asynchronous
predictor, start it in a second terminal and then launch the dashboard with the
environment flag enabled:

```sh
python3 predictor.py
ORBITAL_PREDICTOR=1 ./virtual_camera
```

The bridge uses non-blocking UDP on `127.0.0.1:47001`. It sends only the newest
measurement and accepts predictions only when their confidence is at least 0.55;
the C estimator remains the fallback if Python is unavailable or loses confidence.
A TorchScript model can be supplied with `python3 predictor.py --model path/to/model.pt`.

To use another compiler or add project-specific flags, override the Makefile variables on the command line:

```sh
make CC=clang
make CFLAGS="-std=c11 -Wall -Wextra -Wpedantic -O0 -g"
```

`CFLAGS` replaces the Makefile defaults, so include any flags you still need when overriding it.

## Run

Start the dashboard with:

```sh
./virtual_camera
```

The executable is built for Linux. From Windows, run both commands inside WSL
(WSLg is required for the SDL window):

```powershell
wsl --cd /mnt/d/Claude/SIH/SIH26 ./virtual_camera
```

The application opens an SDL window at 1490 x 820 pixels. Use the controls in the left panel to change the simulation while it is running:

- **Noise** changes the generated signal noise intensity.
- **Haze** blends the feed toward a brighter background level.
- **Gaussian** enables continuous noise in the generated frame.
- **Salt / Pepper** enables random black and white pixel spikes.
- **Max Pan** and **Max Tilt** limit controller output speed.
- **Linear Path**, **Circular Orbit**, and **Random Walk** select the target motion pattern.
- **Manual Target** pauses automatic motion; hold the arrow keys or `WASD` to move the target through the world.

Close the window or press `Escape` to exit.
The window can be resized freely; press `F11` to toggle fullscreen. The dashboard
keeps its layout and mouse controls aligned while scaling to the available display.

Manual movement is useful for testing prediction: enable **Manual Target**, move
the target outside the cyan camera field of view, and keep moving it. The tracker
will enter predictive hold and then the full-world search sweep after missed
detections. Move the target back into a scanned camera view to verify lock and
reacquisition.

## Verify SDL3 Before Building

If the build cannot find SDL3, check that `pkg-config` can see the module:

```sh
pkg-config --modversion sdl3
pkg-config --cflags --libs sdl3
```

The first command should print an SDL3 version. The second should print include and linker flags. If either command fails, install the SDL3 development package or add the directory containing `sdl3.pc` to `PKG_CONFIG_PATH`, for example:

```sh
export PKG_CONFIG_PATH="/path/to/lib/pkgconfig:$PKG_CONFIG_PATH"
make
```

If the program builds but cannot open a window, check that it is running inside a graphical session and that the required SDL video backend is available. Remote or headless sessions may need a desktop display or an appropriate virtual display server.

## Clean Build Outputs

Remove the generated executable with:

```sh
make clean
```

To rebuild from scratch:

```sh
make clean && make
```

## Project Layout

```text
src/app/main.c                         Application loop, SDL setup, and simulation wiring
src/video_gen/video_gen.c/.h           Synthetic target motion and grayscale frame generation
src/cv_tracker/cv_tracker.c/.h         Bright-pixel centroid detection and PID-style control
src/renderer/renderer.c/.h             SDL dashboard and camera-feed rendering
src/gui/gui.c/.h                       Lightweight dashboard controls and labels
src/profiler/profiler.c/.h             Frame and pipeline timing measurements
src/types/types.h                      Shared simulation data structures and constants
src/predictor_bridge/predictor_bridge.c/.h  Optional non-blocking C/Python predictor bridge
predictor.py       Asynchronous constant-velocity/PyTorch predictor
Makefile           Build and clean targets
```

The simulation uses a deterministic internal pseudo-random generator for generated noise. No webcam, video file, network connection, or external runtime asset is required.

## Build Pipeline

Each frame follows this path:

1. Update the target position according to the selected motion pattern.
2. Generate the grayscale camera frame and add enabled disturbances.
3. Search an adaptive predicted-position ROI and expand it for recovery when detection is lost.
4. Update the C state estimator and project the target across measured frame latency.
5. Optionally accept a high-confidence asynchronous Python prediction.
6. If the target remains absent, sweep the camera across the full world in a serpentine pan/tilt scan.
7. Apply proportional, integral, and derivative controller terms, clamped by pan/tilt limits.
8. Render the dashboard and telemetry through SDL3.
