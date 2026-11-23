#include "UI.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdexcept>

UI::~UI()
{
    if (!initialized)
        return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

bool UI::initializeUI(GLFWwindow *window)
{
    if (initialized || window == nullptr)
        return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
    {
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330"))
    {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    initialized = true;
    return true;
}

UIChanges UI::drawUI(WatercolorSettings &settings, bool licEnabled)
{
    if (!initialized)
        throw std::logic_error("Cannot draw the UI before it is initialized");

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Watercolor Adjustments");
    ImGui::SliderInt("Octave count", &settings.octaveCount, 1, 8);
    UIChanges changes;
    changes.perlinSettingsChanged = ImGui::IsItemDeactivatedAfterEdit();
    ImGui::SliderFloat("Persistence", &settings.persistence, 0.0f, 1.0f);
    changes.perlinSettingsChanged |= ImGui::IsItemDeactivatedAfterEdit();
    ImGui::SliderFloat("Lacunarity", &settings.lacunarity, 1.0f, 2.0f);
    changes.perlinSettingsChanged |= ImGui::IsItemDeactivatedAfterEdit();
    if (licEnabled)
    {
        ImGui::Separator();
        ImGui::SliderFloat("LIC bias", &settings.licBias, 0.001f, 0.999f);
        changes.licSettingsChanged = ImGui::IsItemDeactivatedAfterEdit();
        ImGui::SliderFloat("LIC gain", &settings.licGain, 0.001f, 0.999f);
        changes.licSettingsChanged |= ImGui::IsItemDeactivatedAfterEdit();
    }
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return changes;
}
