#pragma once
#include <algorithm>

// One solver step retains its original unit of simulation time. Playback maps
// ten of those units to one wall-clock second, independently of rendering FPS.
class SimulationClock
{
  public:
    static constexpr double stepSeconds = 0.1;
    static constexpr int maxStepsPerFrame = 4;

    int advance(double elapsedSeconds, double speed, bool paused)
    {
        if (paused)
        {
            reset();
            return 0;
        }
        // Discard long stalls (minimize, debugger, window drag), and bound work
        // per frame so an overloaded solver cannot accumulate a catch-up spiral.
        accumulator += std::clamp(elapsedSeconds, 0.0, 0.25) * speed;
        const int due = static_cast<int>((accumulator + 1e-9) / stepSeconds);
        accumulator = std::max(0.0, accumulator - due * stepSeconds);
        return std::min(due, maxStepsPerFrame);
    }

    void reset() { accumulator = 0.0; }

  private:
    double accumulator = 0.0;
};
