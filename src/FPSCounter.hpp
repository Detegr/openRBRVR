#pragma once

#include <array>
#include <numeric>
#include <vector>

// Makes a lazy effort to detect the refresh rate of the HMD
// Not good by any means but works well enough for this purpose
// The idea is to capture frames when the user is first opening the main menu
// It should be light enough for rendering with full refresh rate unless the user
// has some bizarre 8x AA settings active

class FPSCounter {
private:
    bool frameTimesCaptured;
    double targetFrameTime;
    std::vector<double> frameTimes;

public:
    FPSCounter()
        : frameTimesCaptured(false)
        , targetFrameTime(0.0)
        , frameTimes()
    {
    }

    void RecordFrameTime(double frameTime)
    {
        if (frameTime > 0.1) {
            // Discard large (>100ms) values
            return;
        }

        if (!frameTimesCaptured) [[unlikely]] {
            frameTimes.push_back(frameTime);
            if (frameTimes.size() == 300) {
                frameTimesCaptured = true;
            }
        }
    }

    double TargetFrameTime()
    {
        if (!frameTimesCaptured) {
            return 0.0;
        } else if (targetFrameTime == 0.0) {
            double min = *std::ranges::min_element(frameTimes);
            double max = *std::ranges::max_element(frameTimes);
            targetFrameTime = (std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0) - min - max) / (frameTimes.size() - 2);
        }
        return targetFrameTime * 1000.0;
    }
};