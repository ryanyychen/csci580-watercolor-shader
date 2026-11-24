#include "renderer/Render_World.h"
#include "renderer/Image_Output.h"
#include "renderer/Mesh.h"
#include "renderer/ModeLabel.h"
#include "renderer/PigmentLookup.h"
#include "renderer/Shader.h"
#include "renderer/TextureGenerator.h"
#include "renderer/WatercolorSim.h"
#include "renderer/UI.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdlib>

namespace
{
// GLFW callbacks recover this per-window state through the window's user pointer.
// The world owns the scene; input state only keeps a reference to it.
struct Input
{
    Render_World *world = nullptr;
    bool dragging = false;
    bool rotateLight = false;
    bool save_screenshot = false;
    bool show_ui = false;
    double lastX = 0, lastY = 0;
};

Input &input(GLFWwindow *window)
{
    return *static_cast<Input *>(glfwGetWindowUserPointer(window));
}

bool uiCapturesMouse()
{
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
}

bool uiCapturesKeyboard()
{
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard;
}

void freeTexturePixels(float **pixels, int size)
{
    if (pixels == nullptr)
        return;
    for (int y = 0; y < size; ++y)
        std::free(pixels[y]);
    std::free(pixels);
}

struct FloatTextureData
{
    float **rows;
    int size;

    ~FloatTextureData()
    {
        freeTexturePixels(rows, size);
    }
};


Shader * phongShader;
Shader * curveShader;
Shader * watercolorShader;
Shader * directionShader;
Shader * paperShader;

enum class ShaderType {
    Phong,
    CurveMap,
    Watercolor
};

ShaderType currentShader = ShaderType::Phong;

// Keep the on-screen label and window title tied to the actual active mode.
std::string shaderModeLabel(const Render_World &world)
{
    switch (currentShader)
    {
    case ShaderType::Phong:
        return world.show_normals ? "MODE 1/3: PHONG - NORMALS" : "MODE 1/3: PHONG";
    case ShaderType::CurveMap:
        return "MODE 2/3: CURVATURE";
    case ShaderType::Watercolor:
        if (!world.objects.empty() && world.objects.front()->material.useLIC)
            return std::string("MODE 3/3: WATERCOLOR KM - LIC DIR ") +
                   (world.objects.front()->material.principalDirection == PrincipalDirection::Dir1
                        ? "1" : "2");
        return "MODE 3/3: WATERCOLOR KM - PERLIN";
    }
    return "MODE UNKNOWN";
}

void createShaders(const std::filesystem::path &resources) {
    phongShader = new Shader(resources / "shaders/phong.vert", resources / "shaders/phong.frag");
    curveShader = new Shader(resources / "shaders/curvature.vert", resources / "shaders/curvature.frag");
    watercolorShader = new Shader(resources / "shaders/watercolor.vert", resources / "shaders/watercolor.frag");
    directionShader = new Shader(resources / "shaders/direction.vert", resources / "shaders/direction.frag");
    paperShader = new Shader(resources / "shaders/paper.vert", resources / "shaders/paper.frag");
}

void switchShaders(GLFWwindow * window) {
    auto& state = input(window);
    auto& meshes = state.world->objects;
    switch(currentShader) {
        case(ShaderType::Phong):
            currentShader = ShaderType::CurveMap;
            for (auto& obj : meshes) {
                obj->material_shader = curveShader;
            }
            break;
        case(ShaderType::CurveMap):
            currentShader = ShaderType::Watercolor;
            for (auto& obj : meshes) {
                obj->material_shader = watercolorShader;
            }
            break;
        case(ShaderType::Watercolor):
            currentShader = ShaderType::Phong;
            for (auto& obj : meshes) {
                obj->material_shader = phongShader;
            }
            break;
    }
}

void key(GLFWwindow *window, int k, int, int action, int)
{
    // Handle toggles once per press, instead of repeating while a key is held.
    if (action != GLFW_PRESS || uiCapturesKeyboard())
        return;
    if (k == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (k == GLFW_KEY_R)
        input(window).world->camera.Reset();
    if (k == GLFW_KEY_N)
        input(window).world->show_normals = !input(window).world->show_normals;
    if (k == GLFW_KEY_S)
        input(window).save_screenshot = true;
    if (k == GLFW_KEY_I)
        input(window).show_ui = !input(window).show_ui;
    if (k == GLFW_KEY_1 || k == GLFW_KEY_2)
    {
        const PrincipalDirection direction =
            k == GLFW_KEY_1 ? PrincipalDirection::Dir1 : PrincipalDirection::Dir2;
        bool directionChanged = false;
        for (auto &object : input(window).world->objects)
        {
            directionChanged |= object->material.principalDirection != direction;
            object->material.principalDirection = direction;
        }
        if (directionChanged)
            std::cout << "Principal direction: "
                      << (direction == PrincipalDirection::Dir1 ? "dir1" : "dir2")
                      << '\n';
    }
    if (k == GLFW_KEY_L)
    {
        bool licToggled = false;
        bool licEnabled = false;
        for (auto &object : input(window).world->objects)
        {
            const size_t directionIndex =
                static_cast<size_t>(object->material.principalDirection);
            if (object->material.licTextures[directionIndex] ||
                object->material.useTriplanarNoise)
            {
                object->material.useLIC = !object->material.useLIC;
                licToggled = true;
                licEnabled = object->material.useLIC;
            }
        }
        if (licToggled)
            std::cout << "LIC " << (licEnabled ? "enabled" : "disabled") << '\n';
    }
    if (k == GLFW_KEY_SPACE)
        switchShaders(window);
    if (k == GLFW_KEY_C)
        input(window).world->show_vertex_attributes = !input(window).world->show_vertex_attributes;
}

void mouseButton(GLFWwindow *window, int button, int action, int)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT && button != GLFW_MOUSE_BUTTON_RIGHT)
        return;
    auto &state = input(window);
    if (action == GLFW_RELEASE)
    {
        state.dragging = false;
        return;
    }
    if (action != GLFW_PRESS)
        return;

    state.dragging = false;
    if (uiCapturesMouse())
        return;
    state.dragging = true;
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        state.rotateLight = false;
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
        state.rotateLight = true;
    // Start each drag at the current cursor position to avoid a camera jump.
    glfwGetCursorPos(window, &state.lastX, &state.lastY);
}

void cursor(GLFWwindow *window, double x, double y)
{
    auto &state = input(window);
    if (state.dragging && !uiCapturesMouse()) {
        if (!state.rotateLight) {
            state.world->camera.Orbit(static_cast<float>(x - state.lastX),
                                    static_cast<float>(y - state.lastY));
        } else {
            for (auto &light : state.world->lights) {
                light.rotateLight(static_cast<float>(x - state.lastX),
                                    static_cast<float>(y - state.lastY));
            }
        }
    }
        
    state.lastX = x;
    state.lastY = y;
}

void scroll(GLFWwindow *window, double, double y)
{
    if (uiCapturesMouse())
        return;
    input(window).world->camera.Zoom(static_cast<float>(y));
}

bool paperOverlayEnabled(const Render_World &world)
{
    // Keep the comparison modes free of the watercolor paper treatment.
    if (currentShader != ShaderType::Watercolor)
        return false;
    for (const auto &object : world.objects)
    {
        if (object && object->material.useLIC)
            return true;
    }
    return false;
}

void drawPaperOverlay(
    Shader &shader,
    Texture &paperTexture,
    GLuint paperVAO,
    float strength)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader.use();
    paperTexture.Bind(GL_TEXTURE0);
    shader.set("paperTexture", 0);
    shader.set("uPaperStrength", strength);
    glBindVertexArray(paperVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void updateNoiseTexture(
    Mesh &mesh, int minimumGridSize, int textureSize,
    int octaveCount, float persistence, float lacunarity)
{
    if (textureSize <= 0)
        throw std::invalid_argument("Perlin texture size must be positive");
    if (octaveCount <= 0)
        throw std::invalid_argument("Perlin octave count must be positive");
    if (!std::isfinite(persistence) || persistence < 0.0f || persistence > 1.0f)
        throw std::invalid_argument("Perlin persistence must be between zero and one");
    if (!std::isfinite(lacunarity) || lacunarity <= 0.0f)
        throw std::invalid_argument("Perlin lacunarity must be positive and finite");

    const double largestScale = std::max(
        1.0, std::pow(static_cast<double>(lacunarity), octaveCount - 1));
    const double requiredGridSize = std::ceil(6.0 * largestScale) + 1.0;
    if (!std::isfinite(requiredGridSize) ||
        requiredGridSize > std::numeric_limits<int>::max())
        throw std::invalid_argument("Perlin settings require an excessively large gradient grid");
    resizeGradientGrid(std::max(minimumGridSize, static_cast<int>(requiredGridSize)));

    FloatTextureData noiseValues{
        createPerlinTexture(textureSize, octaveCount, persistence, lacunarity),
        textureSize};
    if (noiseValues.rows == nullptr)
        throw std::runtime_error("Failed to allocate Perlin noise texture");
    auto noiseTexture = std::make_unique<Texture>(textureSize, textureSize, noiseValues.rows);
    std::array<std::unique_ptr<Texture>, 2> licTextures;
    std::array<std::array<std::unique_ptr<Texture>, 6>, 2> triplanarLICTextures;

    const PrincipalDirection directions[2] = {
        PrincipalDirection::Dir1, PrincipalDirection::Dir2};
    if (mesh.usesUVNoiseMapping())
    {
        for (size_t directionIndex = 0; directionIndex < 2; ++directionIndex)
        {
            const auto directionField = mesh.createPrincipalDirectionField(
                textureSize, directions[directionIndex]);
            FloatTextureData licValues{
                createLineIntegralConvolutionTexture(
                    textureSize, noiseValues.rows, directionField),
                textureSize};
            if (licValues.rows == nullptr)
                throw std::runtime_error("Failed to allocate LIC texture");
            licTextures[directionIndex] =
                std::make_unique<Texture>(textureSize, textureSize, licValues.rows);
        }
    }
    else
    {
        for (size_t directionIndex = 0; directionIndex < 2; ++directionIndex)
        {
            const auto directionFields = mesh.createTriplanarPrincipalDirectionFields(
                textureSize, directions[directionIndex]);
            for (size_t projectionIndex = 0; projectionIndex < directionFields.size();
                 ++projectionIndex)
            {
                FloatTextureData licValues{
                    createLineIntegralConvolutionTexture(
                        textureSize, noiseValues.rows,
                        directionFields[projectionIndex]),
                    textureSize};
                if (licValues.rows == nullptr)
                    throw std::runtime_error("Failed to allocate triplanar LIC texture");
                triplanarLICTextures[directionIndex][projectionIndex] =
                    std::make_unique<Texture>(textureSize, textureSize, licValues.rows);
            }
        }
    }

    Material &material = mesh.material;
    delete material.texture;
    material.texture = noiseTexture.release();
    for (size_t directionIndex = 0; directionIndex < 2; ++directionIndex)
    {
        delete material.licTextures[directionIndex];
        material.licTextures[directionIndex] = licTextures[directionIndex].release();
        for (size_t projectionIndex = 0; projectionIndex < 6; ++projectionIndex)
        {
            delete material.triplanarLICTextures[directionIndex][projectionIndex];
            material.triplanarLICTextures[directionIndex][projectionIndex] =
                triplanarLICTextures[directionIndex][projectionIndex].release();
        }
    }
    material.useMeshTextureCoordinates = mesh.usesUVNoiseMapping();
    material.useTriplanarNoise = !mesh.usesUVNoiseMapping();
}

void run(GLFWwindow *window, const std::filesystem::path &resources,
         const std::vector<std::filesystem::path> &meshPaths, bool smokeTest,
         const std::filesystem::path &outputPath)
{
    // All GPU resources are created after main() has made an OpenGL context current.
    // Declare the shader before the world so objects are destroyed before their shader.
    createShaders(resources);
    UI noiseUI;
    Render_World world;
    auto mesh = std::make_unique<Mesh>(meshPaths);
    mesh->material_shader = phongShader;
    mesh->direction_shader = directionShader;

    // Scene setup: object material, world lights, and camera.
    // Copper diffuse/ambient with a neutral highlight makes the three Phong
    // contributions readable without an unrelated blue tint in shadowed regions.
    mesh->material.color_diffuse = glm::vec3(0.45f, 0.16f, 0.06f);
    mesh->material.color_ambient = mesh->material.color_diffuse;
    mesh->material.color_specular = glm::vec3(0.60f);
    mesh->material.specular_power = 48.0f;

    // Bake a fixed measured pigment over fixed paper on the CPU. To use a fixed
    // mixture, replace pigment with measuredLayerFromMix(...).pigment here.
    // Changing either pigment or paper requires rebuilding this table.
    const MeasuredPigment pigment = measuredPigmentFromName("Ultramarine Blue");
    const glm::vec3 pigmentPaperColor(0.96f, 0.94f, 0.88f);
    const auto pigmentColors = createMeasuredPigmentLookup(
        pigment, pigmentPaperColor, 1024, mesh->material.pigmentLookupMaxThickness);
    mesh->material.pigmentLookup =
        new Texture(static_cast<int>(pigmentColors.size()), 1, pigmentColors);
    
    // Generate noise textures for watercolor shading.
    WatercolorSettings noiseSettings;
    noiseSettings.licBias = mesh->material.licBias;
    noiseSettings.licGain = mesh->material.licGain;
    constexpr int noiseGridSize = 256;
    const int noiseTextureSize = 256;
    updateNoiseTexture(
        *mesh, noiseGridSize, noiseTextureSize,
        noiseSettings.octaveCount, noiseSettings.persistence, noiseSettings.lacunarity);

    Mesh *meshForNoiseUpdates = mesh.get();
    world.objects.push_back(std::move(mesh));
    // An off-axis white key light gives a clear lit-to-shadow transition.
    // Keep its strength below one so the highlight retains headroom.
    world.lights.front().position = glm::vec3(-3.0f, 4.0f, 5.0f);
    world.lights.front().color = glm::vec3(1.0f);
    world.lights.front().intensity = 0.85f;
    world.ambient_color = glm::vec3(1.0f);
    world.ambient_intensity = 0.10f;
    world.background_color = glm::vec3(0.91f, 0.93f, 0.95f);

    // setup paper texture overlay
    const int paperSize = 256;
    FloatTextureData paperValues{
        createPaperTexture(paperSize, paperSize),
        paperSize
    };
    if (paperValues.rows == nullptr)
        throw std::runtime_error("Failed to allocate paper texture");
    Texture paperTexture(paperSize, paperSize, paperValues.rows);

    GLuint paperVAO = 0;
    glGenVertexArrays(1, &paperVAO);
    if (paperVAO == 0)
        throw std::runtime_error("Failed to create paper overlay VAO");

    ModeLabel modeLabel;
    std::string previousModeLabel;

    // Register callbacks after their input state and scene are ready.
    Input state;
    state.world = &world;
    glfwSetWindowUserPointer(window, &state);
    glfwSetKeyCallback(window, key);
    glfwSetMouseButtonCallback(window, mouseButton);
    glfwSetCursorPosCallback(window, cursor);
    glfwSetScrollCallback(window, scroll);
    if (!noiseUI.initializeUI(window))
        throw std::runtime_error("ImGui initialization failed");

    // Keep the nearest fragment at each pixel, regardless of triangle draw order.
    glEnable(GL_DEPTH_TEST);
    int frames = 0;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        int width, height;

        // Use framebuffer pixels rather than logical window size for high-DPI displays.
        glfwGetFramebufferSize(window, &width, &height);
        if (width == 0 || height == 0)
        {
            glfwWaitEvents();
            continue;
        }
        
        // Render first, then read the back buffer for screenshots before presenting it.
        world.Render(width, height);
        if (paperOverlayEnabled(world))
            drawPaperOverlay(*paperShader, paperTexture, paperVAO, 0.20f);

        // Render the mode label last so it stays legible and is included in exports.
        const std::string label = shaderModeLabel(world);
        if (label != previousModeLabel)
        {
            glfwSetWindowTitle(window, ("Watercolor | " + label + " | Space: next shader").c_str());
            previousModeLabel = label;
        }
        int windowWidth = 0, windowHeight = 0;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        const float pixelScale =
            windowWidth > 0 ? static_cast<float>(width) / windowWidth : 1.0f;
        modeLabel.Draw(*directionShader, label, width, height, pixelScale);

        if (state.show_ui && currentShader == ShaderType::Watercolor)
        {
            const UIChanges changes =
                noiseUI.drawUI(noiseSettings, meshForNoiseUpdates->material.useLIC);
            if (changes.perlinSettingsChanged)
            {
                try
                {
                    updateNoiseTexture(
                        *meshForNoiseUpdates, noiseGridSize, noiseTextureSize,
                        noiseSettings.octaveCount, noiseSettings.persistence,
                        noiseSettings.lacunarity);
                }
                catch (const std::exception &error)
                {
                    std::cerr << "Noise texture update failed: " << error.what() << '\n';
                }
            }
            if (changes.licSettingsChanged)
            {
                meshForNoiseUpdates->material.licBias = noiseSettings.licBias;
                meshForNoiseUpdates->material.licGain = noiseSettings.licGain;
            }
        }

        if (!outputPath.empty() && frames == 0)
            saveScreenshot(world.camera, outputPath);
        
        if (state.save_screenshot)
        {
            // An interactive file error should leave the viewer usable.
            try
            {
                saveScreenshot(world.camera, "output.ppm");
            }
            catch (const std::exception &error)
            {
                std::cerr << "Screenshot failed: " << error.what() << '\n';
            }
            state.save_screenshot = false;
        }

        if (smokeTest)
        {
            // Wait for GPU work so the short validation run observes rendering errors.
            glFinish();
            if (glGetError() != GL_NO_ERROR)
                throw std::runtime_error("OpenGL smoke test failed");
        }

        glfwSwapBuffers(window);
        ++frames;

        if ((smokeTest && frames == 3) || (!smokeTest && !outputPath.empty()))
            break;
    }
    glDeleteVertexArrays(1, &paperVAO);
    // Mesh and shader destruct before their OpenGL context is destroyed.
}
} // namespace

int main(int argc, char **argv)
{
    GLFWwindow *window = nullptr;
    try
    {
        // Root executable -> existing shaders/ and assets/ folders beside it.
        // Explicit model and output paths are resolved from the working directory.
        std::filesystem::path resources = std::filesystem::absolute(argv[0]).parent_path();
        std::vector<std::filesystem::path> meshPaths;
        std::filesystem::path outputPath;
        bool smokeTest = false;
        bool runSim = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help")
            {
                std::cout
                    << "Usage: watercolor [part.obj ...] [--resources directory] [--output image.ppm] [--smoke-test] [--sim]\n"
                       "Left drag: orbit | Wheel: zoom | R: reset | N: normals | S: save output.ppm | Esc: quit\n"
                       "--output renders one frame to P3 PPM and exits; combined with --smoke-test renders three frames.\n"
                       "--sim runs the watercolor simulation and saves images to screenshots/sim/.\n";
                return 0;
            }
            if (arg == "--resources" && i + 1 < argc)
                resources = std::filesystem::absolute(argv[++i]);
            else if (arg == "--output" && i + 1 < argc)
                outputPath = std::filesystem::absolute(argv[++i]);
            else if (arg == "--smoke-test")
                smokeTest = true;
            else if (arg == "--sim")
                runSim = true;
            else if (arg.rfind("--", 0) == 0)
                throw std::runtime_error("Unknown or incomplete option: " + arg);
            else
                meshPaths.push_back(std::filesystem::absolute(arg));
        }
        if (!outputPath.empty() && outputPath.extension() != ".ppm")
            throw std::runtime_error("--output requires a .ppm filename (P3 format)");
        // simulation runs on the CPU and no window.
        if (runSim)
        {
            runWatercolorSimDemo();
            return 0;
        }
        if (meshPaths.empty())
            meshPaths.push_back(resources / "assets/models/octahedron.obj");
        glfwSetErrorCallback([](int code, const char *message)
                             { std::cerr << "GLFW " << code << ": " << message << '\n'; });
        if (!glfwInit())
            throw std::runtime_error("GLFW initialization failed");
        
        // Core profile excludes legacy fixed-function drawing; GLSL handles shading.
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

        if (smokeTest || !outputPath.empty())
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window = glfwCreateWindow(1100, 800, "Watercolor | Phong | N: normals | R: reset | S: save",
                                  nullptr, nullptr);
        if (!window)
            throw std::runtime_error("Cannot create an OpenGL 3.3 window");
        glfwMakeContextCurrent(window);

        // Entry points can only be loaded after a context is current on this thread.
        if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)))
            throw std::runtime_error("GLAD initialization failed");
        glfwSwapInterval(smokeTest || !outputPath.empty() ? 0 : 1);
        // Match the terrain assignment's startup diagnostics so the selected
        // hardware or software renderer is visible, including inside WSL.
        std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << '\n';
        std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << '\n';
        std::cout << "Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION)
                  << '\n';
        run(window, resources, meshPaths, smokeTest, outputPath);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }
    catch (const std::exception &error)
    {
        // Report file/shader/context failures consistently and release window resources.
        std::cerr << "Error: " << error.what() << '\n';
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
}
