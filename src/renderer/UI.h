#ifndef UI_H
#define UI_H

#include <GLFW/glfw3.h>

struct WatercolorSettings
{
    int octaveCount = 4;
    float persistence = 0.5f;
    float lacunarity = 2.0f;
    float licBias = 0.5f;
    float licGain = 0.75f;
};

struct UIChanges
{
    bool perlinSettingsChanged = false;
    bool licSettingsChanged = false;
};

class UI {
    public:
        ~UI();
        bool initializeUI(GLFWwindow *window);
        UIChanges drawUI(WatercolorSettings &settings, bool licEnabled);

    private:
        bool initialized = false;
};

#endif