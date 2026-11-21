#include "renderer/Render_World.h"
#include "renderer/Image_Output.h"
#include "renderer/Mesh.h"
#include "renderer/Shader.h"
#include "renderer/TextureGenerator.h"
#include "renderer/WatercolorSim.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
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
    bool save_screenshot = false;
    double lastX = 0, lastY = 0;
};

Input &input(GLFWwindow *window)
{
    return *static_cast<Input *>(glfwGetWindowUserPointer(window));
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

enum class ShaderType {
    Phong,
    CurveMap,
    Watercolor
};

ShaderType currentShader = ShaderType::Phong;

void createShaders(const std::filesystem::path &resources) {
    phongShader = new Shader(resources / "shaders/phong.vert", resources / "shaders/phong.frag");
    curveShader = new Shader(resources / "shaders/curvature.vert", resources / "shaders/curvature.frag");
    watercolorShader = new Shader(resources / "shaders/watercolor.vert", resources / "shaders/watercolor.frag");
    directionShader = new Shader(resources / "shaders/direction.vert", resources / "shaders/direction.frag");
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
    if (action != GLFW_PRESS)
        return;
    if (k == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (k == GLFW_KEY_R)
        input(window).world->camera.Reset();
    if (k == GLFW_KEY_N)
        input(window).world->show_normals = !input(window).world->show_normals;
    if (k == GLFW_KEY_S)
        input(window).save_screenshot = true;
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
    if (button != GLFW_MOUSE_BUTTON_LEFT)
        return;
    input(window).dragging = action == GLFW_PRESS;
    // Start each drag at the current cursor position to avoid a camera jump.
    glfwGetCursorPos(window, &input(window).lastX, &input(window).lastY);
}

void cursor(GLFWwindow *window, double x, double y)
{
    auto &state = input(window);
    if (state.dragging)
        state.world->camera.Orbit(static_cast<float>(x - state.lastX),
                                  static_cast<float>(y - state.lastY));
    state.lastX = x;
    state.lastY = y;
}

void scroll(GLFWwindow *window, double, double y)
{
    input(window).world->camera.Zoom(static_cast<float>(y));
}


void run(GLFWwindow *window, const std::filesystem::path &resources,
         const std::vector<std::filesystem::path> &meshPaths, bool smokeTest,
         const std::filesystem::path &outputPath)
{
    // All GPU resources are created after main() has made an OpenGL context current.
    // Declare the shader before the world so objects are destroyed before their shader.
    createShaders(resources);
    Render_World world;
    auto mesh = std::make_unique<Mesh>(meshPaths);
    mesh->material_shader = phongShader;
    mesh->direction_shader = directionShader;

    // Scene setup: object material, world lights, and camera.
    mesh->material.color_ambient = glm::vec3(0.02f, 0.02f, 0.8f);
    mesh->material.color_diffuse = glm::vec3(0.8f, 0.02f, 0.02f);
    mesh->material.color_specular = glm::vec3(1.0f);
    mesh->material.specular_power = 64.0f;
    
    // generate noise textures for watercolor shading
    generateGradientGrid();
    const int noiseTextureSize = 256;
    FloatTextureData noiseValues{createPerlinTexture(noiseTextureSize), noiseTextureSize};
    if (noiseValues.rows == nullptr)
        throw std::runtime_error("Failed to allocate Perlin noise texture");
    mesh->material.texture = new Texture(noiseTextureSize, noiseTextureSize, noiseValues.rows);

    const PrincipalDirection directions[2] = {
        PrincipalDirection::Dir1, PrincipalDirection::Dir2};
    if (mesh->usesUVNoiseMapping())
    {
        for (size_t directionIndex = 0; directionIndex < 2; ++directionIndex)
        {
            const auto directionField = mesh->createPrincipalDirectionField(
                noiseTextureSize, directions[directionIndex]);
            FloatTextureData licValues{
                createLineIntegralConvolutionTexture(
                    noiseTextureSize, noiseValues.rows, directionField),
                noiseTextureSize};
            if (licValues.rows == nullptr)
                throw std::runtime_error("Failed to allocate LIC texture");
            mesh->material.licTextures[directionIndex] =
                new Texture(noiseTextureSize, noiseTextureSize, licValues.rows);
        }
        mesh->material.useMeshTextureCoordinates = true;
    }
    else
    {
        for (size_t directionIndex = 0; directionIndex < 2; ++directionIndex)
        {
            const auto directionFields = mesh->createTriplanarPrincipalDirectionFields(
                noiseTextureSize, directions[directionIndex]);
            for (size_t projectionIndex = 0; projectionIndex < directionFields.size();
                 ++projectionIndex)
            {
                FloatTextureData licValues{
                    createLineIntegralConvolutionTexture(
                        noiseTextureSize, noiseValues.rows,
                        directionFields[projectionIndex]),
                    noiseTextureSize};
                if (licValues.rows == nullptr)
                    throw std::runtime_error("Failed to allocate triplanar LIC texture");
                mesh->material.triplanarLICTextures[directionIndex][projectionIndex] =
                    new Texture(noiseTextureSize, noiseTextureSize, licValues.rows);
            }
        }
        mesh->material.useTriplanarNoise = true;
    }

    world.objects.push_back(std::move(mesh));
    world.lights.front().position = glm::vec3(3.0f, 4.0f, 4.0f);
    world.lights.front().color = glm::vec3(0.3f);
    world.ambient_intensity = 0.12f;

    // Register callbacks after their input state and scene are ready.
    Input state;
    state.world = &world;
    glfwSetWindowUserPointer(window, &state);
    glfwSetKeyCallback(window, key);
    glfwSetMouseButtonCallback(window, mouseButton);
    glfwSetCursorPosCallback(window, cursor);
    glfwSetScrollCallback(window, scroll);

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
                       "--sim runs the watercolor simulation and saves screenshots/sim/step000.ppm.\n";
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
