#pragma once

#include <chrono>
#include <cstdint>

#include "RBR.hpp"

// Optional per-run performance logger. Disabled by default (config key `perfLog`).
//
// One summary record is appended to Plugins\openRBRVR\perf\runs.csv and runs.jsonl for each
// stage run (first driving frame -> stage end / restart / quit to menu / game exit).
// With `perfLogDetail` a per-second CSV is written for the run as well.
//
// While recording, the only per-frame work is updating a few counters and fixed size
// histograms. All file I/O happens when the run ends. See docs/PerfLog.md.
namespace perflog {
    // Everything the logger needs to know about one presented frame
    struct FrameSample {
        std::chrono::steady_clock::time_point frame_start;
        std::chrono::steady_clock::time_point frame_end;
        float cpu_ms = 0.0f; // frame_start -> just before Present, same as the debug overlay
        float gpu_ms = 0.0f; // <= 0 when there is no new GPU measurement
        float wait_ms = 0.0f; // OpenVR: wait for present, OpenXR: time spent in xrWaitFrame
        float present_ms = 0.0f; // OpenVR only: present call
        float display_delta_ms = 0.0f; // OpenXR only: delta between predicted display times
        bool late = false; // OpenVR only: the compositor reported a dropped or mispresented frame
        bool skipped = false; // OpenXR only: shouldRender was false
    };

    // Called every frame from the render hook with the current game state.
    // Starts and ends runs based on game mode and stage changes.
    void update(rbr::GameMode mode, uint32_t stage_id, uint32_t car_id);

    // Called once per presented frame
    void on_frame(const FrameSample& sample);

    // True if a run is being recorded and the game is currently driving
    bool is_recording();

    // End the current run (if any) and write the records
    void end_run(const char* reason);
}
