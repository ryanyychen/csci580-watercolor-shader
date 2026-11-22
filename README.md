# Watercolor Renderer — CSCI 580

## Project summary

A Computer Graphics final project exploring stylistic watercolor rendering and
physical water/pigment simulation. The C++/OpenGL OBJ viewer supports Phong lighting,
curvature visualization, and three watercolor views using measured pigment colors.
It includes camera controls, an on-screen mode label, and PPM image output.

The physical simulation currently runs separately on a flat 2D paper grid. It is
not yet connected to the 3D model or the viewer's watercolor shader.

## Five viewing options

There are **five visual options**, implemented with three mesh shader programs:
Phong, curvature, and watercolor. Perlin and the two LIC directions are settings
of the same watercolor shader.

| Option | What you see | How to select it |
| --- | --- | --- |
| **1. Phong** | Copper material with ambient, diffuse, and specular lighting; the baseline for checking shape and illumination. | Starting view; **Space** cycles back to it. |
| **2. Curvature** | Deep blue for low bending, approaching white for strong bending. Uses the larger absolute principal curvature, so inward and outward bending can look the same. | **Space** to curvature. |
| **3. Watercolor: Perlin** | Irregular pigment variation from Perlin noise, with more pigment in shaded regions. | **Space** to watercolor; use **L** to select Perlin if LIC is active. |
| **4. Watercolor: LIC direction 1** | Pigment streaks following the first principal curvature direction. | In watercolor, use **L** to enable LIC, then **1**. |
| **5. Watercolor: LIC direction 2** | The same watercolor treatment, with streaks following the second principal curvature direction. | With LIC active, press **2**. |

**Space cycles Phong → Curvature → Watercolor → Phong.** It does not cycle through
all five options individually. **L** switches Perlin/LIC within watercolor, and
**1 / 2** select the LIC direction. The on-screen label and window title identify
the active view. **C** overlays principal direction lines for inspection.

### How the views connect

All five use the same mesh, camera, and transforms. Phong provides a lighting
baseline; the watercolor shader uses its own lighting calculation to control
pigment thickness rather than reading the Phong image.

The CPU computes principal curvature values and directions when loading the mesh.
The curvature shader displays the **values**. LIC uses the **directions** to filter
Perlin noise into surface-aligned streaks:

```text
Mesh geometry → principal curvature calculation
                 ├─ values → blue/white curvature view (diagnostic only)
                 └─ directions → LIC direction 1 or 2
Perlin noise ───────────────────→ LIC filtering

Lighting + Perlin OR LIC noise → pigment thickness → measured KM lookup → watercolor color
```

The curvature image is not an input to watercolor. Perlin and both LIC views share
the same pigment lookup and thickness calculation; they differ in the noise used
to distribute pigment. LIC also enables the paper-grain overlay.
These views generate a static paint appearance, not time-dependent water flow.

## Code trace

Running `./watercolor "path/to/model.obj"` follows this path:

```text
STARTUP — once
main.cpp                 Parse arguments; open the GLFW/OpenGL window.
Shader.cpp               Compile the shader programs.
Mesh.cpp                 Load/triangulate OBJ parts; center and scale the model;
                         compute normals and curvature; upload GPU geometry.
Mesh.cpp +
TextureGenerator.cpp     Build Perlin noise and both curvature-aligned LIC textures.
PigmentLookup.cpp        Bake spectral KM colors into a 1024x1 GPU lookup texture.

RENDER LOOP — every frame
main.cpp                 Handle camera controls and view selection.
Render_World.cpp         Send transforms, lights, material, and textures to the GPU.
Active vertex shader     Transform vertices; pass attributes to the fragment stage.
OpenGL                   Rasterize triangles; interpolate attributes; depth-test.
Active fragment shader   Phong lighting, curvature colors, or watercolor color.
main.cpp + ModeLabel.cpp Draw the optional paper overlay and mode label; show frame.
Camera.cpp +
Image_Output.cpp         Read the framebuffer and save PPM when requested.
```

The frame repeats as you move the camera. Passing multiple OBJ paths combines
their geometry before centering/scaling, preserving their relative positions.
With no path, the viewer loads `assets/models/octahedron.obj`. All imported parts
currently share one material; OBJ/MTL colors and textures are not applied.

Running `./watercolor --sim` runs the physical simulation instead of the viewer:

```text
main.cpp                 Select the CPU simulation; skip the window.
WatercolorSim.cpp         Generate paper heights and paint an initial wet circle.
                         Repeat 100 steps:
                           updateVelocities → relaxDivergence → flowOutward
                           → movePigment
KubelkaMunk.cpp           Convert pigment amounts to French Ultramarine over paper.
Image_Output.cpp         Save paper.ppm, wet.ppm, and step images in screenshots/sim/.
```

The simulation follows Curtis et al. (1997) on a 2D grid of paper cells, with water
velocities stored on the cell edges (a staggered grid). It uses the same paper texture
as the stylistic overlay, and its colors come from the Kubelka-Munk module. Water now
flows and carries pigment within the initial wet region, accumulating pigment near
its edge. `updateVelocities` handles motion and paper slopes; `relaxDivergence`
reduces flow imbalance; `flowOutward` lowers pressure near the wet edge; and
`movePigment` transports suspended pigment. The wet mask does not yet expand into
dry cells. Settling, granulation, and drying are also unfinished, so later frames
keep moving pigment toward the edge.

Connecting this simulation to the model still requires a surface mapping (such as
a UV atlas), seam-aware transport and surface distances, expanding water coverage,
and live pigment-texture updates in the render loop. The watercolor shader would
then read simulated pigment amounts instead of generating thickness from noise.

### Pigment color modules

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

[src/renderer/KubelkaMunkMeasured.h](src/renderer/KubelkaMunkMeasured.h) offers the same
functions with pigments measured at 36 wavelengths (Okumura 2005), so mixes follow the
real pigments more closely, for example ultramarine versus phthalo blue mixed with yellow.

```cpp
MeasuredPigment blue = measuredPigmentFromName("Ultramarine Blue");
glm::vec3 color = colorFromMeasuredLayers({{blue, 0.5f}}, paperColor);
```

The measured data comes from the
[painting_tools](https://github.com/rubenwiersma/painting_tools) repository and is shared
under CC BY-NC-SA 4.0. 
The simulation uses the RGB version. The watercolor shader samples a lookup table
generated by the measured spectral version on the CPU; it does not evaluate the
36-band equations in GLSL.

### Where to make changes

| File | What it controls |
| --- | --- |
| [SConstruct](SConstruct) | Compilation, dependencies, debug/release settings, and the root executable. |
| [src/main.cpp](src/main.cpp) | Startup, input callbacks, scene setup, material colors, and light settings. |
| [src/renderer/Object.h](src/renderer/Object.h) | Material fields and per-object model transform. Values assigned in `main.cpp` override material defaults. |
| [src/renderer/Mesh.cpp](src/renderer/Mesh.cpp) | OBJ loading, UV and triplanar principal-direction field baking, triangulation, normals, and GPU geometry. |
| [src/renderer/Camera.cpp](src/renderer/Camera.cpp) | View/projection matrices, orbit, zoom, and framebuffer readback. |
| [src/renderer/Render_World.cpp](src/renderer/Render_World.cpp) | Per-frame drawing and passing scene/material values to shader uniforms. |
| [src/renderer/Shader.cpp](src/renderer/Shader.cpp) | GLSL compilation/linking and uniform upload helpers. |
| [src/renderer/TextureGenerator.cpp](src/renderer/TextureGenerator.cpp) | Generates Perlin noise and line-integral-convolution filtering for the watercolor shading pass. |
| [src/renderer/PigmentLookup.cpp](src/renderer/PigmentLookup.cpp) | Bakes measured spectral pigment reflectance into a thickness-to-linear-RGB lookup table. |
| [phong.vert](shaders/phong.vert), [phong.frag](shaders/phong.frag) | Transform geometry/normals and calculate ambient, diffuse, and specular lighting. |
| [curvature.vert](shaders/curvature.vert), [curvature.frag](shaders/curvature.frag) | Pass principal curvature values to the fragment shader and map bending magnitude to blue/white. |
| [watercolor.vert](shaders/watercolor.vert), [watercolor.frag](shaders/watercolor.frag) | Map Perlin/LIC noise onto the surface; combine it with lighting to choose thickness and sample the pigment lookup. |
| [paper.vert](shaders/paper.vert), [paper.frag](shaders/paper.frag) | Draw a fullscreen paper-grain overlay in the LIC watercolor views. |
| [src/renderer/ModeLabel.cpp](src/renderer/ModeLabel.cpp) | Draws the current shader mode and switching hint inside the window and screenshots. |
| [src/renderer/Image_Output.cpp](src/renderer/Image_Output.cpp) | Write the rendered framebuffer to a P3 PPM image. |
| [src/renderer/WatercolorSim.cpp](src/renderer/WatercolorSim.cpp) | Physical watercolor simulation: paper grid, wet area, water, and pigment. |
| [src/renderer/KubelkaMunk.cpp](src/renderer/KubelkaMunk.cpp) | Kubelka-Munk pigment colors: turns layers of pigment over paper into the final RGB color. |
| [src/renderer/KubelkaMunkMeasured.cpp](src/renderer/KubelkaMunkMeasured.cpp) | The same, computed per wavelength with measured pigment data. |
| [src/renderer/KubelkaMunkCore.h](src/renderer/KubelkaMunkCore.h) | Kubelka-Munk formulas shared by both versions. |

### Watercolor texture mapping and controls

For meshes with complete OBJ UVs within one tile, the watercolor pass uses a Perlin
texture and two LIC versions, aligned with `dir1` and `dir2`, from the same source.
All three views use the same measured pigment; LIC changes the pigment distribution.
Meshes without complete UVs use six signed-axis projections; their Perlin
and LIC samples are blended according to the surface normal, so the cow can use
LIC without an authored UV atlas. UVs extending beyond one tile use this
triplanar mapping too, avoiding compression of the noise over oversized UV ranges.
The LIC output applies post-LIC bias and gain (starting defaults: `0.55` and
`0.70`) for stronger, brush-like streak contrast. Set either value to `0.5`
for a neutral curve; change `mesh->material.licBias` and `licGain` to tune it.
These controls do not affect the Perlin-only view.

When LIC is active in watercolor mode, the viewer also draws a cream-colored paper texture over the
rendered window. The grain is generated by
[src/renderer/TextureGenerator.cpp](src/renderer/TextureGenerator.cpp) and blended
at 20% strength in front of the scene. Press **L** to hide the overlay along with
LIC by switching back to the unfiltered Perlin texture.

### Material and lighting settings

For Phong colors, edit `mesh->material` in `src/main.cpp`: `color_diffuse` is the main
surface color, `color_ambient` affects the ambient contribution, `color_specular`
controls highlights, and `specular_power` controls their sharpness. RGB components
use values from 0 to 1. Edit `world.lights` and `world.ambient_intensity` to change
illumination.

For watercolor colors, edit the `pigment` and `pigmentPaperColor` setup in
`src/main.cpp` (default: measured Ultramarine Blue over warm paper). A fixed mix
can use `measuredLayerFromMix({{blue, 0.7f}, {yellow, 0.3f}}).pigment` instead of a
single pigment. The C++ spectral module generates 1,024 samples, including bare
paper at thickness zero and paint at `pigmentLookupMaxThickness` (default 3).
`Texture` uploads the result as `GL_RGB32F`, with no 8-bit quantization or gamma
encoding. The shader interpolates between texel centers and applies display gamma
only after looking up the linear RGB result.

`watercolorThickness` controls overall paint amount; `watercolorNoiseStrength`
controls how strongly Perlin/LIC redistributes it. Less illuminated regions get
more pigment. These material settings are independent of the Phong RGB colors.
The lookup assumes one fixed pigment mixture, paper color, and illuminant.
Changing mixture, paper, or its thickness range requires regenerating the table;
arbitrary per-fragment mixtures and full spectral lighting are not implemented.
Colored lights currently affect paint amount through luminance. The paper overlay
is an additional display effect, not a per-fragment change to the baked paper.

The Phong baseline uses a copper material with a low ambient contribution, a white
specular highlight, and a neutral white key light.

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

The setup script fetches GLFW, GLAD, GLM, tinyobjloader, and Eigen into `.deps/`.
On Linux, the build links against the system GLFW. WSL needs a graphical session,
such as WSLg, to display the viewer.

**Build and run:**

```sh
scons -j4
./watercolor "path/to/model.obj"

# Render one image and exit
./watercolor "path/to/model.obj" --output screenshots/render.ppm

# Run the physical simulation and save images to screenshots/sim/ (no window)
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
| C | Toggle curvature direction visualization |
| 1 / 2 | Select principal direction `dir1` / `dir2` for LIC |
| S | Save `output.ppm` in the working directory |
| Space | Cycle Phong → Curvature → Watercolor |
| L | Toggle between Perlin and LIC textures in the watercolor shader |
| Esc | Exit |

Other commands: `scons check` checks camera math, PPM output, LIC filtering, and
spectral pigment lookup endpoints/interpolation for both a pigment and a fixed mix;
`scons mode=release -j4` builds an optimized executable, and `scons -c` cleans
the default build. Append `--smoke-test` to a viewer command for a short hidden-window
graphics check; `--help` lists the command-line options.

## Research and similar project

- **Rusinkiewicz (2004)** — [Estimating Curvatures and Their Derivatives on Triangle Meshes](https://pixl.cs.princeton.edu/pubs/Rusinkiewicz_2004_ECA/curvpaper.pdf).
  Used to estimate principal curvatures and principal curvature directions on the mesh.
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
Our viewer uses rasterization for the stylistic views; the physical simulation
currently runs separately on a 2D grid. Connecting it to a rendered scene remains
future work.
