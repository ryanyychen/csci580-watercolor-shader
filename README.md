# Watercolor Renderer — CSCI 580

## Project summary

A Computer Graphics final project comparing stylistic watercolor shading with
physical water/pigment simulation on shared 3D scenes. The current implementation
is a C++/OpenGL OBJ viewer with Phong lighting, camera controls, and PPM image output.
Both watercolor methods are planned; instructor approval of the proposed
image-space physical simulation is still pending.

## Code trace

Running `./watercolor "path/to/model.obj"` follows this path:

```text
main.cpp: parse model path and create the GLFW/OpenGL context
  -> Shader.cpp: read and compile phong.vert + phong.frag
  -> Mesh.cpp: read OBJ -> triangulate -> prepare normals -> center/scale
               -> upload positions/normals into a GPU vertex buffer
  -> Render_World.cpp: pass camera, object material, and lights to the shader
  -> phong.vert: transform vertices and normals into world/clip space
  -> OpenGL: rasterize triangles and interpolate attributes
  -> phong.frag: calculate ambient + diffuse + specular lighting
  -> OpenGL: use depth testing to determine visible fragments in the framebuffer
  -> main.cpp: present the rendered frame in the window
```

The frame repeats as you move the camera. Passing multiple OBJ paths combines
their geometry before centering/scaling, preserving their relative positions.
With no path, the viewer loads `assets/models/octahedron.obj`. All imported parts
currently share one material; OBJ/MTL colors and textures are not applied.

Running `./watercolor --sim` runs the physical simulation instead of the viewer:

```text
main.cpp: see --sim and skip the window (the simulation runs on the CPU)
  -> WatercolorSim.cpp: make the paper height field from Perlin noise + random grain
  -> WatercolorSim.cpp: paint one wet circle of water and pigment
  -> Image_Output.cpp: save screenshots/sim/step000.ppm
```

The simulation follows Curtis et al. (1997) on a 2D grid of paper cells. So far it
only sets up the paper and the brush stroke; water flow, pigment movement, and drying
are next, and its colors will then come from the Kubelka-Munk module.

Both watercolor methods can turn pigment amounts into colors with
[src/renderer/KubelkaMunk.h](src/renderer/KubelkaMunk.h), which follows the
Kubelka-Munk model in Curtis et al. (1997). A pigment comes from the Curtis palette
or from an RGB color, and layers are listed from bottom (painted first) to top:

```cpp
Pigment blue = pigmentFromName("French Ultramarine");
Pigment yellow = pigmentFromRGB(glm::vec3(0.9f, 0.8f, 0.2f));
glm::vec3 color = colorFromLayers({{blue, 0.5f}, {yellow, 0.3f}}, paperColor);
```

`layerFromMix` mixes several pigments into one layer, as in wet-in-wet painting.
Neither the simulation nor the watercolor shader uses this module yet.

| File | What it controls |
| --- | --- |
| [SConstruct](SConstruct) | Compilation, dependencies, debug/release settings, and the root executable. |
| [src/main.cpp](src/main.cpp) | Startup, input callbacks, scene setup, material colors, and light settings. |
| [src/renderer/Object.h](src/renderer/Object.h) | Material fields and per-object model transform. Values assigned in `main.cpp` override material defaults. |
| [src/renderer/Mesh.cpp](src/renderer/Mesh.cpp) | OBJ loading, triangulation, normal fallback, shared bounds, and GPU geometry. |
| [src/renderer/Camera.cpp](src/renderer/Camera.cpp) | View/projection matrices, orbit, zoom, and framebuffer readback. |
| [src/renderer/Render_World.cpp](src/renderer/Render_World.cpp) | Per-frame drawing and passing scene/material values to shader uniforms. |
| [src/renderer/Shader.cpp](src/renderer/Shader.cpp) | GLSL compilation/linking and uniform upload helpers. |
| [shaders/phong.vert](shaders/phong.vert) | Position transforms and inverse-transpose normal transformation. |
| [shaders/phong.frag](shaders/phong.frag) | Lighting calculations and display gamma encoding; the starting point for stylistic watercolor effects. |
| [src/renderer/Image_Output.cpp](src/renderer/Image_Output.cpp) | Write the rendered framebuffer to a P3 PPM image. |
| [src/renderer/WatercolorSim.cpp](src/renderer/WatercolorSim.cpp) | Physical watercolor simulation: paper grid, wet area, water, and pigment. |
| [src/renderer/KubelkaMunk.cpp](src/renderer/KubelkaMunk.cpp) | Kubelka-Munk pigment colors: turns layers of pigment over paper into the final RGB color. |

For colors, edit `mesh->material` in `src/main.cpp`: `color_diffuse` is the main
surface color, `color_ambient` affects the ambient contribution, `color_specular`
controls highlights, and `specular_power` controls their sharpness. RGB components
use values from 0 to 1. Edit `world.lights` and `world.ambient_intensity` to change
illumination.

Rebuild after C++ changes. Shader edits take effect when you restart the viewer.

## Build and run

Requires Python 3.9+, Git, a C++17 compiler, and OpenGL 3.3 or newer.
Linux/WSL also needs the system GLFW development package and `pkg-config`.
Run these commands from the repository root.

**Ubuntu / WSL setup:**

```sh
sudo apt update
sudo apt install build-essential pkg-config libglfw3-dev python3-venv

python3 -m venv .venv          # Only if you do not already have an environment
source .venv/bin/activate
python -m pip install -r requirements-build.txt
python scripts/setup_dependencies.py
```

The setup script fetches GLFW, GLAD, GLM, and tinyobjloader into `.deps/`.
On Linux, the build links against the system GLFW. WSL needs a graphical session,
such as WSLg, to display the viewer.

**Build and run:**

```sh
scons -j4
./watercolor "path/to/model.obj"

# Render one image and exit
./watercolor "path/to/model.obj" --output screenshots/render.ppm

# Run the physical simulation and save screenshots/sim/step000.ppm (no window)
./watercolor --sim
```

The executable is at the repository root; intermediate files stay in
`build-scons/`. Paths containing spaces must be quoted.

**Windows:** install MinGW-w64 and Python, run the same pip/dependency-setup
commands, then `python -m SCons -j4` and `.\watercolor.exe "C:\path\to\model.obj"`.
For MSVC, use Developer PowerShell and add `toolchain=msvc` to the build command.

| Control | Action |
| --- | --- |
| Left drag / mouse wheel | Orbit / zoom |
| R | Reset camera |
| N | Toggle normal visualization |
| S | Save `output.ppm` in the working directory |
| Space | Switch between the Phong and watercolor shaders |
| Esc | Exit |

Other commands: `scons check` checks camera math and PPM output,
`scons mode=release -j4` builds an optimized executable, and `scons -c` cleans
the default build. Append `--smoke-test` to a viewer command for a short hidden-window
graphics check; `--help` lists the command-line options.

## Research and similar project

- **Lum and Ma (2001)** — [Non-Photorealistic Rendering using Watercolor Inspired
  Textures and Illumination](https://escholarship.org/uc/item/2g48f8kn).
  Main reference for lighting-driven, texture-based stylistic watercolor.
- **Curtis et al. (1997)** — [Computer-Generated Watercolor (PDF)](https://grail.cs.washington.edu/wp-content/uploads/2015/08/curtis-1997-cgw.pdf).
  Main reference for physical water flow, pigment behavior, and paint compositing.
- **Bousseau et al. (2006)** — [Interactive Watercolor Rendering with Temporal
  Coherence and Abstraction](https://www.matt-kaplan.com/npr/watercolor/).
  Reference for image/3D watercolor effects and stability during camera motion.
- **Cheng (2017)** — [Art Directed Watercolor Shader for Non-Photorealistic Rendering
  with a Focus on Reflections](https://oaktrust.library.tamu.edu/items/91754b92-360e-4540-a29e-3a68c2ac4be3).
  Supporting reference for artist-controlled surface shading.
- **Praun et al. (2001)** — [Real-Time Hatching (PDF)](https://ics.uci.edu/~gopi/ICS280Win02/RealTimeHatching.pdf).
  Tonal art maps are an alternative stylistic approach; adapting them to watercolor
  would be our implementation work.

**Similar project:** [Watercolor-esque Non-Photorealistic Rendering](https://andrewdotwang.github.io/WatercolorRendering/)
by Andrew Wang, Evan Lohn, and Johnathan Zhou (UC Berkeley CS184, Spring 2020).
[Source code](https://github.com/andrewdotwang/WatercolorRendering).
Their project builds on ray tracing and mesh-based water/pigment transport.
Our proposed approach uses a shared rasterization renderer to compare stylistic
watercolor with image-space physical simulation.
