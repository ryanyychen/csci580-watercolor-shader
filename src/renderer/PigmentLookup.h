#pragma once

#include "KubelkaMunkMeasured.h"
#include <vector>

// Linear-RGB reflectance samples from thickness 0 to maxThickness, inclusive.
// The pigment (or fixed mixture) and paper stay constant across this table.
// Regenerate it when either changes; thickness can vary freely in the shader.
std::vector<glm::vec3> createMeasuredPigmentLookup(
    const MeasuredPigment &pigment, const glm::vec3 &paperColor,
    int sampleCount = 1024, float maxThickness = 3.0f);
