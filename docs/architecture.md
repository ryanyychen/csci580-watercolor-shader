# Renderer and integration boundaries

Current path:

`OBJ -> CPU triangulation/normals/bounds -> GPU vertex buffer -> Phong shaders -> window`

`Render_World` follows the neighboring Raytracer project: a camera, owned objects,
point lights, and ambient settings. `Mesh` derives from `Object`, with
`material_shader`, `number_parts`, a material, and a model transform. Material
colors and `specular_power` follow the course `Phong_Shader`. Object lifetime is
owned through unique pointers, while shader references are non-owning.

The camera retains Raytracer's position/basis/film fields and setup methods,
using GLM and a vector-owned packed RGBA `colors` buffer. Its horizontal FOV is
in radians. The OpenGL projection matches the same film coordinates and pixel
centers; resize updates aspect ratio and buffer size. Orbit/zoom update the
camera through `Position_And_Aim_Camera`.

Image output adapts the course `Dump_ppm` and terrain `saveScreenshot` paths.
After rendering and before buffer swapping, read the back buffer as RGBA bytes,
pack into course Pixels, and write P3 PPM with rows reversed. S captures the
interactive view to output.ppm; --output writes one frame and exits. Readback
uses framebuffer dimensions and restores pixel alignment/read-buffer state.
These are display RGB screenshots, not linear simulation buffers.

Positions and normals reach the fragment shader in world space. The normal
matrix uses inverse transpose; the fragment shader renormalizes interpolated
normals. Lighting uses ambient + Lambert diffuse + reflection-vector Phong
specular. Up to eight point lights have constant intensity (no distance attenuation or
shadows). The display pass applies approximate gamma 2.2 encoding.

The loader combines shapes from one or more OBJ files into a single non-indexed draw call, preserving
per-corner supplied normals. Missing/zero/invalid normals fall back to the face
normal; this gives flat shading, not smoothing-group reconstruction. It skips
degenerate triangles and normalizes the visible bounds to a longest dimension
of 2 units centered at the origin. Multiple files retain their relative positions:
normalization happens once over their combined bounds, not once per file.
The parts must already be exported in a shared coordinate system. All parts
currently use one material and one model transform; separately animated parts
would need separate scene objects. UVs, MTL materials, textures, skeletal
animation, and model hierarchies are outside this milestone. Export triangulated
OBJ for reliable results on concave polygons.

Next shared-renderer milestone: introduce a render target with linear RGBA
color, a separate object mask, world normals, and depth. Keep gamma encoding in
the final display pass only. All methods must use the same camera, model,
lighting, dimensions, and background. Define whether depth is hardware depth or
linear view-space distance before consuming it.

The stylistic pipeline will operate on the scene shading and/or render targets.
The physical pipeline currently has a separate CPU 2D paper-grid solver,
`WatercolorSim`. `--sim` opens `SimulationViewer`, which uses the existing GLFW
context and ImGui backend to display a texture built from `imagePixels()`.
The export path consumes the same pixels. `SimulationClock` schedules fixed solver
steps at 10 Hz by default, with adjustable playback speed and bounded catch-up;
solver steps retain their original unit rather than using variable render deltas.
`--sim-offline` retains the windowless 100-step demo. Consuming scene color/mask
and returning pigment for model compositing remain integration plans.

Suggested four-person split after the baseline: two people on shared rendering
and stylistic effects, two on simulation and compositing. Agree on buffer
formats together before either pipeline depends on them. No individual
assignments or calendar deadlines have been set here.
