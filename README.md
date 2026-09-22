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

This compiles every C source file in `src/` with the following default options:

```text
-std=c11 -Wall -Wextra -Wpedantic -O2
```

The resulting executable is created at:

```text
./virtual_camera
```

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

The application opens an SDL window at 1490 x 820 pixels. Use the controls in the left panel to change the simulation while it is running:

- **Noise** changes the generated signal noise intensity.
- **Haze** blends the feed toward a brighter background level.
- **Gaussian** enables continuous noise in the generated frame.
- **Salt / Pepper** enables random black and white pixel spikes.
- **Max Pan** and **Max Tilt** limit controller output speed.
- **Linear Path**, **Circular Orbit**, and **Random Walk** select the target motion pattern.

Close the window or press `Escape` to exit.

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
src/main.c         Application loop, SDL setup, and simulation wiring
src/video_gen.c    Synthetic target motion and grayscale frame generation
src/cv_tracker.c   Bright-pixel centroid detection and PID-style control
src/renderer.c     SDL dashboard and camera-feed rendering
src/gui.c          Lightweight dashboard controls and labels
src/profiler.c     Frame and pipeline timing measurements
src/types.h        Shared simulation data structures and constants
Makefile           Build and clean targets
```

The simulation uses a deterministic internal pseudo-random generator for generated noise. No webcam, video file, network connection, or external runtime asset is required.

## Build Pipeline

Each frame follows this path:

1. Update the target position according to the selected motion pattern.
2. Generate the grayscale camera frame and add enabled disturbances.
3. Find pixels at or above the brightness threshold of 200.
4. Calculate the bright-pixel centroid and tracking error.
5. Apply proportional, integral, and derivative controller terms, clamped by pan/tilt limits.
6. Render the dashboard and telemetry through SDL3.
