#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "Dx.hpp"
#include "Globals.hpp"
#include "PerfLog.hpp"
#include "Util.hpp"
#include "Version.hpp"

#ifndef OPENRBRVR_COMMIT
#define OPENRBRVR_COMMIT "unknown"
#endif

namespace perflog {
    using clock = std::chrono::steady_clock;

    // Fixed size histogram for percentiles. Memory use does not depend on run length.
    // Bin width 0.05ms, range 0..102.4ms. Longer values land in the last bin, the exact maximum is tracked separately.
    struct Histogram {
        static constexpr float bin_ms = 0.05f;
        static constexpr size_t bins = 2048;

        std::array<uint32_t, bins> counts {};
        uint64_t n = 0;
        double sum = 0.0;
        float max = 0.0f;

        void add(float ms)
        {
            const auto idx = std::min<size_t>(static_cast<size_t>(std::max(ms, 0.0f) / bin_ms), bins - 1);
            counts[idx]++;
            n++;
            sum += ms;
            max = std::max(max, ms);
        }

        double mean() const { return n ? sum / static_cast<double>(n) : 0.0; }

        double percentile(double q) const
        {
            if (n == 0) {
                return 0.0;
            }
            const auto target = std::max<uint64_t>(static_cast<uint64_t>(std::ceil(q * static_cast<double>(n))), 1);
            uint64_t acc = 0;
            for (size_t i = 0; i < bins; i++) {
                acc += counts[i];
                if (acc >= target) {
                    return std::min<double>((static_cast<double>(i) + 0.5) * bin_ms, max);
                }
            }
            return max;
        }
    };

    // The N slowest frames of the run with their position on the driving clock (seconds spent driving since the run started)
    struct Worst {
        static constexpr size_t count = 10;
        struct Entry {
            float ms = 0.0f;
            float t = 0.0f;
        };
        std::array<Entry, count> entries {};

        void add(float ms, float t)
        {
            if (ms <= entries.back().ms) {
                return;
            }
            size_t i = count - 1;
            while (i > 0 && entries[i - 1].ms < ms) {
                entries[i] = entries[i - 1];
                i--;
            }
            entries[i] = { ms, t };
        }

        std::string str() const
        {
            std::string out;
            for (const auto& e : entries) {
                if (e.ms > 0.0f) {
                    out += std::format("{}{:.1f}s={:.1f}ms", out.empty() ? "" : ";", e.t, e.ms);
                }
            }
            return out;
        }
    };

    struct SecondRow {
        uint32_t t;
        uint32_t frames;
        float cpu_mean, cpu_max;
        float gpu_mean, gpu_max;
        uint32_t gpu_samples;
    };

    struct Run {
        bool active = false;
        std::time_t start_time = 0;
        clock::time_point start;
        clock::time_point last_update;
        double paused_s = 0.0;

        // Snapshot of the render settings at the start of the run
        uint32_t stage_id = 0;
        uint32_t car_id = 0;
        bool btb = false;
        std::string context;
        uint32_t res[4][2] = {};
        float supersampling = 1.0f;
        int msaa = 0;
        int peripheral_msaa = 0;
        bool quad_view = false;
        bool multiview = false;
        std::string runtime;
        std::string runtime_info;
        bool detail = false;

        uint64_t frames = 0;
        uint64_t over_budget = 0;
        uint64_t over_70 = 0;
        uint64_t budget_frames = 0; // frames for which a frame budget was known
        double budget_sum = 0.0;
        double hmd_period_sum = 0.0;
        uint64_t hmd_period_frames = 0;
        uint64_t late_frames = 0;
        uint64_t skipped_frames = 0;
        float last_gpu_ms = 0.0f;
        Histogram cpu, gpu, frame, interval, wait, present;
        Worst worst_frame, worst_interval;

        bool prev_valid = false;
        clock::time_point prev_frame_start;

        // Per-second aggregation, only used with perfLogDetail
        std::vector<SecondRow> rows;
        uint32_t sec_frames = 0, sec_gpu_samples = 0;
        double sec_cpu_sum = 0, sec_gpu_sum = 0;
        float sec_cpu_max = 0, sec_gpu_max = 0;
        uint32_t next_second = 1;
    };

    static Run run;
    constexpr size_t max_detail_rows = 4 * 3600;
    constexpr uint64_t min_frames = 30;

    static std::filesystem::path perf_dir()
    {
        return std::filesystem::path("Plugins") / "openRBRVR" / "perf";
    }

    static std::string iso_utc(std::time_t t)
    {
        std::tm tm {};
        gmtime_s(&tm, &t);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
        return buf;
    }

    static float drive_clock(clock::time_point now)
    {
        return static_cast<float>(std::chrono::duration<double>(now - run.start).count() - run.paused_s);
    }

    static void start_run(uint32_t stage_id, uint32_t car_id)
    {
        if (!g::vr) {
            return;
        }
        run = Run {};
        run.active = true;
        run.start_time = std::time(nullptr);
        run.start = run.last_update = clock::now();
        run.stage_id = stage_id;
        run.car_id = car_id;
        run.btb = rbr::is_on_btb_stage();
        run.detail = g::cfg.perf_log_detail;
        if (run.detail) {
            run.rows.reserve(max_detail_rows); // no reallocations while driving
        }

        const auto ctx = g::vr->get_current_render_context();
        run.context = g::vr->get_current_render_context_name();
        run.quad_view = g::vr->is_using_quad_view_rendering();
        run.multiview = dx::multiview_rendering_enabled();
        run.runtime = g::vr->get_runtime_type() == OPENVR ? "openvr" : "openxr";
        run.runtime_info = g::vr->get_runtime_info();
        run.msaa = static_cast<int>(ctx->msaa);
        run.peripheral_msaa = run.quad_view ? static_cast<int>(g::cfg.peripheral_msaa) : 0;
        const auto cfg_ctx = g::cfg.gfx.find(run.context);
        run.supersampling = cfg_ctx != g::cfg.gfx.end() ? cfg_ctx->second.supersampling : g::cfg.supersampling;
        for (auto tgt : { LeftEye, RightEye, FocusLeft, FocusRight }) {
            const auto [w, h] = g::vr->get_render_resolution(tgt);
            run.res[tgt][0] = w;
            run.res[tgt][1] = h;
        }
    }

    bool is_recording()
    {
        return run.active && rbr::get_game_mode() == rbr::GameMode::Driving;
    }

    void update(rbr::GameMode mode, uint32_t stage_id, uint32_t car_id)
    {
        if (!g::cfg.perf_log) {
            if (run.active) {
                end_run("disabled");
            }
            return;
        }

        if (run.active) {
            const auto now = clock::now();
            if (mode != rbr::GameMode::Driving) {
                const auto dt = std::chrono::duration<double>(now - run.last_update).count();
                run.paused_s += dt;
                run.prev_valid = false;
            }
            run.last_update = now;

            // Pause, replay, blackout and the start countdown are part of the run, everything else ends it
            const char* reason = nullptr;
            if (stage_id != run.stage_id) {
                reason = "stage_change";
            } else if (mode == rbr::GameMode::End) {
                reason = "finish";
            } else if (mode == rbr::GameMode::PreStage) {
                reason = "restart";
            } else if (mode == rbr::GameMode::MainMenu) {
                reason = "menu";
            } else if (mode == rbr::GameMode::Loading) {
                reason = "loading";
            } else if (mode == rbr::GameMode::Exiting || mode == rbr::GameMode::Quit) {
                reason = "quit";
            }
            if (reason) {
                end_run(reason);
            }
        }

        if (!run.active && mode == rbr::GameMode::Driving) {
            start_run(stage_id, car_id);
        }
    }

    static void flush_second()
    {
        if (run.sec_frames == 0) {
            return;
        }
        if (run.rows.size() < max_detail_rows) {
            run.rows.push_back(SecondRow {
                .t = run.next_second,
                .frames = run.sec_frames,
                .cpu_mean = static_cast<float>(run.sec_cpu_sum / run.sec_frames),
                .cpu_max = run.sec_cpu_max,
                .gpu_mean = run.sec_gpu_samples ? static_cast<float>(run.sec_gpu_sum / run.sec_gpu_samples) : 0.0f,
                .gpu_max = run.sec_gpu_max,
                .gpu_samples = run.sec_gpu_samples,
            });
        }
        run.sec_frames = run.sec_gpu_samples = 0;
        run.sec_cpu_sum = run.sec_gpu_sum = 0;
        run.sec_cpu_max = run.sec_gpu_max = 0;
    }

    void on_frame(const FrameSample& s)
    {
        auto& r = run;
        const bool has_gpu = s.gpu_ms > 0.0f;
        const float interval_ms = r.prev_valid ? std::chrono::duration<float, std::milli>(s.frame_start - r.prev_frame_start).count() : 0.0f;
        r.prev_valid = true;
        r.prev_frame_start = s.frame_start;

        if (r.detail) {
            const auto elapsed = static_cast<uint32_t>(std::chrono::duration<double>(s.frame_end - r.start).count());
            if (elapsed >= r.next_second) {
                flush_second();
                r.next_second = elapsed + 1;
            }
            r.sec_frames++;
            r.sec_cpu_sum += s.cpu_ms;
            r.sec_cpu_max = std::max(r.sec_cpu_max, s.cpu_ms);
            if (has_gpu) {
                r.sec_gpu_samples++;
                r.sec_gpu_sum += s.gpu_ms;
                r.sec_gpu_max = std::max(r.sec_gpu_max, s.gpu_ms);
            }
        }

        r.frames++;
        r.cpu.add(s.cpu_ms);
        if (has_gpu) {
            r.gpu.add(s.gpu_ms);
            // The GPU time is not available for every frame with OpenXR, carry the last value over like the debug overlay does
            r.last_gpu_ms = s.gpu_ms;
        }
        if (interval_ms > 0.0f) {
            r.interval.add(interval_ms);
        }
        if (s.wait_ms > 0.0f) {
            r.wait.add(s.wait_ms);
        }
        if (s.present_ms > 0.0f) {
            r.present.add(s.present_ms);
        }

        // Same definition of frame time as the "FPS only" debug overlay
        const auto frame_ms = std::max(s.cpu_ms, r.last_gpu_ms);
        r.frame.add(frame_ms);
        const auto t = drive_clock(s.frame_end);
        r.worst_frame.add(frame_ms, t);
        r.worst_interval.add(interval_ms, t);

        // Frame budget: HMD display period if the runtime tells it, otherwise the highest FPS seen (like the overlay)
        const float period = g::vr->get_display_period_ms();
        float budget = period;
        if (period > 0.0f) {
            r.hmd_period_sum += period;
            r.hmd_period_frames++;
        } else if (g::target_fps > 0) {
            budget = 1000.0f / static_cast<float>(g::target_fps);
        }
        if (budget > 0.0f) {
            r.budget_frames++;
            r.budget_sum += budget;
            r.over_budget += frame_ms > budget;
            r.over_70 += frame_ms > budget * 0.7f;
        }

        r.late_frames += s.late || (period > 0.0f && s.display_delta_ms > period * 1.5f);
        r.skipped_frames += s.skipped;
    }

    using Fields = std::vector<std::pair<std::string, std::string>>;

    static void stat_fields(Fields& f, const std::string& name, const Histogram& h)
    {
        f.emplace_back(name + "_mean_ms", std::format("{:.2f}", h.mean()));
        f.emplace_back(name + "_p50_ms", std::format("{:.2f}", h.percentile(0.50)));
        f.emplace_back(name + "_p90_ms", std::format("{:.2f}", h.percentile(0.90)));
        f.emplace_back(name + "_p99_ms", std::format("{:.2f}", h.percentile(0.99)));
        f.emplace_back(name + "_max_ms", std::format("{:.2f}", h.max));
    }

    // Make a string safe to put into an unquoted CSV field and a JSON string
    static std::string clean(std::string s)
    {
        std::erase(s, '\0');
        for (auto& c : s) {
            if (c == ',' || c == '\n' || c == '\r') {
                c = ';';
            } else if (c == '"' || c == '\\') {
                c = '\'';
            }
        }
        return s;
    }

    // Everything else is written as a JSON number, or null if empty
    static bool is_text_field(const std::string& key)
    {
        constexpr std::array text = {
            "timestamp", "end_timestamp", "end_local_date", "end_local_time", "version", "commit", "dxvk", "xr_runtime", "gpu",
            "stage_name", "end_reason", "render_context", "res_left", "res_right", "res_focus_left", "res_focus_right",
            "qvf_user_settings", "runtime", "budget_source", "worst_frames", "worst_intervals", "detail_file"
        };
        return std::ranges::find(text, key) != text.end();
    }

    // Stage name, length and surface from Maps\Tracks.ini
    static void stage_info(Fields& f, uint32_t stage_id)
    {
        std::string name, length, surface;
        try {
            ini::IniFile tracks("Maps\\Tracks.ini");
            const auto section = std::format("Map{:02}", stage_id);
            if (tracks.find(section) != tracks.end()) {
                const auto get = [&](const char* key) {
                    auto it = tracks[section].find(key);
                    return it != tracks[section].end() ? it->second.as<std::string>() : std::string();
                };
                const auto unquote = [](std::string s) {
                    std::erase(s, '"');
                    return s;
                };
                name = unquote(get("StageName"));
                length = get("Length");
                surface = get("Surface");
            }
        } catch (...) {
            dbg("Failed to read stage info from Tracks.ini");
        }
        f.emplace_back("stage_name", clean(name));
        f.emplace_back("stage_length_km", clean(length));
        f.emplace_back("stage_surface", clean(surface));
    }

    // User overrides of the Quad-Views-Foveated layer, flattened to one line with the section name as a prefix
    // (`SteamVR.peripheral_multiplier=0.85`). Empty if there are none.
    static std::string qvf_user_settings()
    {
        char* appdata = nullptr;
        size_t len = 0;
        std::string out;
        if (_dupenv_s(&appdata, &len, "LOCALAPPDATA") == 0 && appdata) {
            std::ifstream f(std::filesystem::path(appdata) / "Quad-Views-Foveated" / "settings.cfg");
            std::string line, section;
            while (std::getline(f, line)) {
                if (line.empty() || line[0] == '#' || line[0] == ';') {
                    continue;
                }
                if (line[0] == '[') {
                    section = line.substr(1, line.find(']') - 1) + ".";
                } else {
                    out += (out.empty() ? "" : ";") + section + line;
                }
            }
            free(appdata);
        }
        return clean(out);
    }

    static void gpu_info(Fields& f)
    {
        std::string name;
        IDirect3D9* d3d = nullptr;
        if (g::d3d_dev && g::d3d_dev->GetDirect3D(&d3d) == D3D_OK && d3d) {
            D3DADAPTER_IDENTIFIER9 id {};
            if (d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &id) == D3D_OK) {
                name = id.Description;
            }
            d3d->Release();
        }
        f.emplace_back("gpu", clean(name));
    }

    static void append_records(const Fields& f)
    {
        const auto dir = perf_dir();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        const auto csv_path = dir / "runs.csv";
        const auto write_header = !std::filesystem::exists(csv_path, ec) || std::filesystem::file_size(csv_path, ec) == 0;
        if (std::ofstream csv(csv_path, std::ios::app); csv.good()) {
            if (write_header) {
                for (size_t i = 0; i < f.size(); i++) {
                    csv << (i ? "," : "") << f[i].first;
                }
                csv << '\n';
            }
            for (size_t i = 0; i < f.size(); i++) {
                csv << (i ? "," : "") << f[i].second;
            }
            csv << '\n';
        } else {
            dbg("Failed to write perf/runs.csv");
        }

        if (std::ofstream json(dir / "runs.jsonl", std::ios::app); json.good()) {
            json << '{';
            for (size_t i = 0; i < f.size(); i++) {
                json << (i ? "," : "") << '"' << f[i].first << "\":";
                if (is_text_field(f[i].first)) {
                    json << '"' << f[i].second << '"';
                } else {
                    json << (f[i].second.empty() ? "null" : f[i].second);
                }
            }
            json << "}\n";
        } else {
            dbg("Failed to write perf/runs.jsonl");
        }
    }

    static std::string write_detail_file()
    {
        std::tm tm {};
        gmtime_s(&tm, &run.start_time);
        char stamp[32];
        std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tm);
        const auto name = std::format("stage{}-{}.csv", run.stage_id, stamp);

        std::error_code ec;
        std::filesystem::create_directories(perf_dir(), ec);
        if (std::ofstream f(perf_dir() / name); f.good()) {
            f << "t_s,frames,cpu_mean_ms,cpu_max_ms,gpu_mean_ms,gpu_max_ms,gpu_samples\n";
            for (const auto& row : run.rows) {
                f << std::format("{},{},{:.2f},{:.2f},{:.2f},{:.2f},{}\n", row.t, row.frames, row.cpu_mean, row.cpu_max, row.gpu_mean, row.gpu_max, row.gpu_samples);
            }
            return name;
        }
        dbg("Failed to write perf detail file");
        return "";
    }

    void end_run(const char* reason)
    {
        if (!run.active) {
            return;
        }
        run.active = false;
        if (run.frames < min_frames) {
            run = Run {};
            return;
        }

        std::string detail_file;
        if (run.detail) {
            flush_second();
            detail_file = write_detail_file();
        }

        const auto now = clock::now();
        const auto duration_s = std::chrono::duration<double>(now - run.start).count();
        const auto budget_ms = run.budget_frames ? run.budget_sum / static_cast<double>(run.budget_frames) : 0.0;
        const auto period_ms = run.hmd_period_frames ? run.hmd_period_sum / static_cast<double>(run.hmd_period_frames) : 0.0;
        const auto pct = [&](uint64_t n) { return run.frames ? 100.0 * static_cast<double>(n) / static_cast<double>(run.frames) : 0.0; };

        Fields f;
        // Keys for joining with other data sources: UTC start, local end time in the same format as the
        // NGPCarMenu RaceStat database (RaceDate, RaceDateTime), stage id and car slot
        const auto end_time = std::time(nullptr);
        std::tm end_local {};
        localtime_s(&end_local, &end_time);
        char end_date[16], end_clock[16];
        std::strftime(end_date, sizeof(end_date), "%Y%m%d", &end_local);
        std::strftime(end_clock, sizeof(end_clock), "%H%M%S", &end_local);

        f.emplace_back("timestamp", iso_utc(run.start_time));
        f.emplace_back("end_timestamp", iso_utc(end_time));
        f.emplace_back("end_local_date", end_date);
        f.emplace_back("end_local_time", end_clock);
        f.emplace_back("version", VERSION_STR);
        f.emplace_back("commit", OPENRBRVR_COMMIT);
        f.emplace_back("dxvk", clean(g::dxvk_version));
        f.emplace_back("xr_runtime", clean(run.runtime_info));
        gpu_info(f);
        f.emplace_back("stage_id", std::to_string(run.stage_id));
        stage_info(f, run.stage_id);
        f.emplace_back("car_id", std::to_string(run.car_id));
        f.emplace_back("btb", run.btb ? "1" : "0");
        f.emplace_back("end_reason", reason);
        f.emplace_back("duration_s", std::format("{:.1f}", duration_s));
        f.emplace_back("paused_s", std::format("{:.1f}", run.paused_s));
        f.emplace_back("driving_s", std::format("{:.1f}", drive_clock(now)));
        f.emplace_back("frames", std::to_string(run.frames));
        f.emplace_back("render_context", run.context);
        f.emplace_back("res_left", std::format("{}x{}", run.res[LeftEye][0], run.res[LeftEye][1]));
        f.emplace_back("res_right", std::format("{}x{}", run.res[RightEye][0], run.res[RightEye][1]));
        f.emplace_back("res_focus_left", run.quad_view ? std::format("{}x{}", run.res[FocusLeft][0], run.res[FocusLeft][1]) : "");
        f.emplace_back("res_focus_right", run.quad_view ? std::format("{}x{}", run.res[FocusRight][0], run.res[FocusRight][1]) : "");
        f.emplace_back("supersampling", std::format("{:.2f}", run.supersampling));
        f.emplace_back("msaa", std::to_string(run.msaa));
        f.emplace_back("peripheral_msaa", std::to_string(run.peripheral_msaa));
        f.emplace_back("quad_view", run.quad_view ? "1" : "0");
        f.emplace_back("qvf_user_settings", run.quad_view ? qvf_user_settings() : "");
        f.emplace_back("multiview", run.multiview ? "1" : "0");
        f.emplace_back("runtime", run.runtime);
        f.emplace_back("hmd_refresh_hz", period_ms > 0.0 ? std::format("{:.1f}", 1000.0 / period_ms) : "0");
        f.emplace_back("budget_ms", std::format("{:.2f}", budget_ms));
        f.emplace_back("budget_source", run.hmd_period_frames == run.budget_frames && run.budget_frames ? "hmd" : "observed_fps");
        stat_fields(f, "cpu", run.cpu);
        stat_fields(f, "gpu", run.gpu);
        f.emplace_back("gpu_samples", std::to_string(run.gpu.n));
        stat_fields(f, "frame", run.frame);
        stat_fields(f, "interval", run.interval);
        stat_fields(f, "wait", run.wait);
        stat_fields(f, "present", run.present);
        f.emplace_back("over_budget_frames", std::to_string(run.over_budget));
        f.emplace_back("over_budget_pct", std::format("{:.2f}", pct(run.over_budget)));
        f.emplace_back("over_70pct_frames", std::to_string(run.over_70));
        f.emplace_back("over_70pct_pct", std::format("{:.2f}", pct(run.over_70)));
        f.emplace_back("late_frames", std::to_string(run.late_frames));
        f.emplace_back("late_pct", std::format("{:.2f}", pct(run.late_frames)));
        f.emplace_back("skipped_frames", std::to_string(run.skipped_frames));
        f.emplace_back("worst_frames", run.worst_frame.str());
        f.emplace_back("worst_intervals", run.worst_interval.str());
        f.emplace_back("detail_file", detail_file);

        append_records(f);
        run = Run {};
    }
}
