#include "SimulationViewer.h"
#include "SimulationClock.h"
#include "WatercolorSim.h"
#include "UI.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <algorithm>
#include <stdexcept>

namespace
{
struct DisplayTexture
{
    GLuint id = 0;
    DisplayTexture()
    {
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA,
                     GL_UNSIGNED_INT_8_8_8_8, nullptr);
    }
    ~DisplayTexture() { glDeleteTextures(1, &id); }
};
}

void runSimulationViewer(GLFWwindow *window, bool smokeTest,
                         const std::filesystem::path &outputPath)
{
    UI ui;
    if (!ui.initializeUI(window))
        throw std::runtime_error("ImGui initialization failed");
    glfwSetWindowTitle(window, "Watercolor | 2D simulation | Space: pause | R: reset");
    WatercolorSim sim(256, 256);
    sim.makePaper();
    // Preserve this paper on reset, so the same experiment can be replayed.
    const auto paper = sim.paperHeight;
    SimulationClock clock;
    int steps = 0;
    auto reset = [&]() {
        sim = WatercolorSim(256, 256);
        sim.paperHeight = paper;
        sim.paintCircle(128.0f, 128.0f, 60.0f, 1.0f, 0.5f);
        steps = 0;
        clock.reset();
    };
    reset();
    DisplayTexture texture;
    bool paused = false;
    bool dirty = true;
    float speed = 1.0f;
    double previousTime = glfwGetTime();
    int frames = 0;
    std::string saveStatus;
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        const double now = glfwGetTime();
        const double elapsed = now - previousTime;
        previousTime = now;
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        if (width == 0 || height == 0)
        {
            clock.reset();
            glfwWaitEventsTimeout(0.1);
            previousTime = glfwGetTime();
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        bool singleStep = false;
        bool save = false;
        bool resetRequested = false;
        if (!ImGui::GetIO().WantCaptureKeyboard)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
            {
                paused = !paused;
                clock.reset();
            }
            resetRequested = ImGui::IsKeyPressed(ImGuiKey_R, false);
            save = ImGui::IsKeyPressed(ImGuiKey_S, false);
            singleStep = ImGui::IsKeyPressed(ImGuiKey_RightArrow, false);
        }

        const auto *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("2D watercolor", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("2D WATERCOLOR SIMULATION");
        if (ImGui::Button(paused ? "Play (Space)" : "Pause (Space)"))
        {
            paused = !paused;
            clock.reset();
        }
        ImGui::SameLine();
        singleStep |= ImGui::Button("Step (Right)");
        ImGui::SameLine();
        resetRequested |= ImGui::Button("Reset (R)");
        ImGui::SameLine();
        save |= ImGui::Button("Save (S)");
        ImGui::SetNextItemWidth(220.0f);
        ImGui::SliderFloat("Playback speed", &speed, 0.1f, 4.0f, "%.1fx");
        if (resetRequested)
        {
            reset();
            dirty = true;
            saveStatus.clear();
        }
        if (singleStep)
            paused = true;
        // Smoke tests deliberately exercise solver and texture updates even
        // when the three hidden frames take less than one clock interval.
        const int due = smokeTest ? 1 : clock.advance(
            resetRequested ? 0.0 : elapsed, speed, paused);
        const int count = singleStep ? 1 : due;
        for (int i = 0; i < count; ++i)
        {
            sim.step();
            ++steps;
            dirty = true;
        }
        if (dirty)
        {
            const auto pixels = sim.imagePixels();
            glBindTexture(GL_TEXTURE_2D, texture.id);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sim.width, sim.height,
                            GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pixels.data());
            dirty = false;
        }
        ImGui::Text("Step %d | Playback time %.1f s | %s", steps,
                    steps * SimulationClock::stepSeconds, paused ? "Paused" : "Playing");
        int wetCells = 0;
        int dampCells = 0;
        for (int k = 0; k < sim.width * sim.height; k++)
        {
            if (sim.wetMask[k] == 1)
                wetCells++;
            else if (sim.waterInPaper[k] > 0.0f)
                dampCells++;
        }
        ImGui::Text("Wet cells %d | Damp cells %d", wetCells, dampCells);
        ImGui::TextWrapped("Click on the paper to drop clean water.");
        if (save)
        {
            try
            {
                sim.saveImage("screenshots/sim/current.ppm");
                saveStatus = "Saved screenshots/sim/current.ppm";
            }
            catch (const std::exception &error)
            {
                saveStatus = error.what();
            }
        }
        if (!saveStatus.empty())
            ImGui::TextWrapped("%s", saveStatus.c_str());
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float side = std::max(1.0f, std::min(available.x, available.y));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (available.x - side) * 0.5f));
        // Simulation pixels start at the bottom, whereas ImGui UVs start at the top.
        ImGui::Image(static_cast<ImTextureID>(texture.id), ImVec2(side, side),
                      ImVec2(0, 1), ImVec2(1, 0));
        if (ImGui::IsItemClicked())
        {
            // A drop of clean water where the paper was clicked, to show backruns on damp paint
            // and rings on dry paint.
            const float dropRadius = 16.0f;
            const float dropWater = 2.0f;
            const ImVec2 corner = ImGui::GetItemRectMin();
            const ImVec2 mouse = ImGui::GetMousePos();
            const float x = (mouse.x - corner.x) / side * sim.width;
            const float y = (1.0f - (mouse.y - corner.y) / side) * sim.height;
            sim.paintCircle(x, y, dropRadius, dropWater, 0.0f);
            dirty = true;
        }
        ImGui::End();
        glViewport(0, 0, width, height);
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (!outputPath.empty() && frames == 0)
            sim.saveImage(outputPath.string());
        if (smokeTest)
        {
            glFinish();
            if (glGetError() != GL_NO_ERROR)
                throw std::runtime_error("2D simulation OpenGL smoke test failed");
        }
        glfwSwapBuffers(window);
        ++frames;
        if ((smokeTest && frames == 3) || (!smokeTest && !outputPath.empty()))
            break;
    }
}
