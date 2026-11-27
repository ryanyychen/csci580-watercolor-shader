#pragma once
#include <filesystem>

struct GLFWwindow;

void runSimulationViewer(GLFWwindow *window, bool smokeTest,
                         const std::filesystem::path &outputPath);
