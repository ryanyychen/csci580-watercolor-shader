#pragma once
#include "Camera.h"
#include <string>
#include <vector>

// 2D paper grid for the physical watercolor simulation (Curtis et al. 1997)
// cell (i, j) is stored at [j * width + i], bottom row first
class WatercolorSim
{
  public:
    WatercolorSim(int w, int h);

    void makePaper();
    void paintCircle(float centerX, float centerY, float radius, float water, float pigment);
    void saveImage(const std::string &filename);
    std::vector<Pixel> imagePixels() const;
    void saveDebugImages(const std::string &folder);
    float totalPigment();

    // one time step of the main loop: move the water and the pigment, then let the paper soak up water and dry
    void step();
    void updateVelocities();
    void relaxDivergence();
    void flowOutward();
    void movePigment();
    void moveWaterOnPaper();
    void transferPigment();
    void absorbWater();
    void diffuseWater();
    void evaporateWater();

    int width;
    int height;

    std::vector<float> paperHeight;  // h
    std::vector<int> wetMask;        // M
    std::vector<float> pressure;     // p
    std::vector<float> pigmentWater; // g, pigment floating in the water
    std::vector<float> pigmentPaper; // d, pigment settled on the paper

    // water in a cell
    // a stroke puts water on top of the paper (waterOnPaper), where it flows and carries pigment
    // each step the paper soaks up some of it (waterInPaper), and some of the water evaporates
    // a cell is
    //   wet  (wetMask 1) while it has waterOnPaper
    //   damp (wetMask 0) when that is gone but it still has waterInPaper: nothing flows, but more water can spread into it
    //   dry  (wetMask 0) when it has no water left
    // a cell whose paper is nearly full also counts as wet, so that water can flow onto it (see wetThreshold)
    std::vector<float> waterOnPaper; // how much water stands on top of the paper
    std::vector<float> waterInPaper; // s, how much water the paper has soaked up, at most waterCapacity

    // water velocity on the cell edges (staggered grid)
    // u is (width + 1) x height, v is width x (height + 1)
    std::vector<float> u;
    std::vector<float> v;

  private:
    // u at the left edge and v at the bottom edge of cell (i, j), 0 outside the paper
    float uAt(int i, int j);
    float vAt(int i, int j);
    float maxSpeed();
    // moves an amount kept per cell, such as pigmentWater, along with the water flow
    void carryWithFlow(std::vector<float> &amount);
    // c, the most water the paper of cell k can soak up
    float waterCapacity(int k) const;
    // whether waterInPaper of cell k is above the share wetThreshold of its waterCapacity
    bool paperIsNearlyFull(int k) const;
    void enforceBoundaries();
    std::vector<float> blurWetMask();
};

// called by --sim
void runWatercolorSimDemo();
