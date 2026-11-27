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

    // one time step of the main loop: move the water, then the pigment
    void step();
    void updateVelocities();
    void relaxDivergence();
    void flowOutward();
    void movePigment();
    void transferPigment();
    void absorbWater();
    void evaporateWater();

    int width;
    int height;

    std::vector<float> paperHeight;  // h
    std::vector<int> wetMask;        // M
    std::vector<float> pressure;     // p
    std::vector<float> pigmentWater; // g, pigment floating in the water
    std::vector<float> pigmentPaper; // d, pigment settled on the paper

    // Water in a cell
    // A stroke puts water on top of the paper (waterOnPaper), where it flows and carries pigment
    // Each step the paper soaks up some of it (waterInPaper), and some of the water evaporates
    // A cell is
    //   wet  (wetMask 1) while it has waterOnPaper, or more waterInPaper than wetThreshold
    //   damp (wetMask 0) while it still has some waterInPaper: nothing flows, the pigment stays where it is
    //   dry  (wetMask 0) when it has no water left
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
    // c, the most water the paper of cell k can soak up
    float waterCapacity(int k) const;
    void enforceBoundaries();
    std::vector<float> blurWetMask();
};

// called by --sim
void runWatercolorSimDemo();
