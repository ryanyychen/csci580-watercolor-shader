#pragma once
#include "Camera.h"
#include <filesystem>

// Adapted from Dump_ppm in ../Raytracer/main.cpp and ../GPUDriver/main.cpp.
// Expects width * height packed RGBA pixels, ordered from the bottom row upward.
void Dump_ppm(const Pixel *data, int width, int height, const std::filesystem::path &filename);

// Capture display RGB from the active back buffer, then write it as a P3 PPM.
void saveScreenshot(Camera &camera, const std::filesystem::path &filename);
