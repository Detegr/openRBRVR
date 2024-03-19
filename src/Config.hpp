#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

#include <d3d9.h>

#include "Util.hpp"

#include <vec2.hpp>
#include <vec3.hpp>

#define TOML_HEADER_ONLY 1
#include <toml.hpp>

#include "VR.hpp"

static auto int_or_default(const std::string& value, int def) -> int
{
    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        return def;
    }
}

struct Config {
    float menu_size = 1.0;
    float overlay_size = 1.0;
    glm::vec3 overlay_translation = { 0, 0, 0 };
    float supersampling = 1.0;
    HorizonLock lock_to_horizon = HorizonLock::LOCK_NONE;
    double horizon_lock_multiplier = 1.0;
    CompanionMode companion_mode;
    bool draw_loading_screen = true;
    bool debug = false;
    int debug_mode = 0;
    bool render_mainmenu_3d = false;
    bool render_pausemenu_3d = true;
    bool render_prestage_3d = false;
    bool render_replays_3d = false;
    D3DMULTISAMPLE_TYPE msaa = D3DMULTISAMPLE_NONE;
    int anisotropy = 0;
    bool wmr_workaround = false;
    VRRuntime runtime = VRRuntime::OPENVR;
    std::unordered_map<std::string, std::tuple<float, std::vector<int>>> gfx = { { "default", std::make_tuple(1.0f, std::vector<int> {}) } };
    glm::ivec2 companion_offset;
    int companion_size = 100;
    RenderTarget companion_eye = LeftEye;
    int world_scale = 1000;

    Config& operator=(const Config& rhs)
    {
        menu_size = rhs.menu_size;
        overlay_size = rhs.overlay_size;
        overlay_translation = rhs.overlay_translation;
        supersampling = rhs.supersampling;
        lock_to_horizon = rhs.lock_to_horizon;
        horizon_lock_multiplier = rhs.horizon_lock_multiplier;
        companion_mode = rhs.companion_mode;
        draw_loading_screen = rhs.draw_loading_screen;
        debug = rhs.debug;
        render_mainmenu_3d = rhs.render_mainmenu_3d;
        render_pausemenu_3d = rhs.render_pausemenu_3d;
        render_prestage_3d = rhs.render_prestage_3d;
        render_replays_3d = rhs.render_replays_3d;
        wmr_workaround = rhs.wmr_workaround;
        runtime = rhs.runtime;
        msaa = rhs.msaa;
        anisotropy = rhs.anisotropy;
        gfx = rhs.gfx;
        companion_offset = rhs.companion_offset;
        companion_size = rhs.companion_size;
        companion_eye = rhs.companion_eye;
        companion_mode = rhs.companion_mode;
        debug_mode = rhs.debug_mode;
        world_scale = rhs.world_scale;
        return *this;
    }

    bool operator==(const Config& rhs) const
    {
        return menu_size == rhs.menu_size
            && overlay_size == rhs.overlay_size
            && overlay_translation == rhs.overlay_translation
            && supersampling == rhs.supersampling
            && lock_to_horizon == rhs.lock_to_horizon
            && horizon_lock_multiplier == rhs.horizon_lock_multiplier
            && companion_mode == rhs.companion_mode
            && draw_loading_screen == rhs.draw_loading_screen
            && debug == rhs.debug
            && debug_mode == rhs.debug_mode
            && render_mainmenu_3d == rhs.render_mainmenu_3d
            && render_pausemenu_3d == rhs.render_pausemenu_3d
            && render_prestage_3d == rhs.render_prestage_3d
            && render_replays_3d == rhs.render_replays_3d
            && wmr_workaround == rhs.wmr_workaround
            && runtime == rhs.runtime
            && companion_offset == rhs.companion_offset
            && companion_size == rhs.companion_size
            && companion_eye == rhs.companion_eye
            && world_scale == rhs.world_scale;
    }

    bool Write(const std::filesystem::path& path) const
    {
        constexpr auto round = [](double v) -> double { return std::round(v * 100.0) / 100.0; };

        std::ofstream f(path);
        if (!f.good()) {
            return false;
        }
        toml::table out {
            { "menuSize", round(menu_size) },
            { "overlaySize", round(overlay_size) },
            { "overlayTranslateX", round(overlay_translation.x) },
            { "overlayTranslateY", round(overlay_translation.y) },
            { "overlayTranslateZ", round(overlay_translation.z) },
            { "lockToHorizon", static_cast<int>(lock_to_horizon) },
            { "horizonLockMultiplier", round(horizon_lock_multiplier) },
            { "desktopWindowMode", companion_mode_str(companion_mode) },
            { "drawLoadingScreen", draw_loading_screen },
            { "debug", debug },
            { "debugMode", debug_mode },
            { "renderMainMenu3d", render_mainmenu_3d },
            { "renderPauseMenu3d", render_pausemenu_3d },
            { "renderPreStage3d", render_prestage_3d },
            { "renderReplays3d", render_replays_3d },
            { "runtime", runtime == OPENVR ? "steamvr" : (wmr_workaround ? "openxr-wmr" : "openxr") },
            { "desktopWindowOffsetX", companion_offset.x },
            { "desktopWindowOffsetY", companion_offset.y },
            { "desktopWindowSize", companion_size },
            { "desktopEye", static_cast<int>(companion_eye) },
        };

        toml::table gfxTbl;
        for (const auto& v : gfx) {
            toml::table t;
            toml::array a;

            double superSampling = std::get<0>(v.second);
            const std::vector<int>& stages = std::get<1>(v.second);
            for (const auto& stage : stages) {
                a.push_back(stage);
            }

            t.insert("superSampling", round(superSampling));
            if (std::string(v.first) != "default") {
                t.insert("stages", a);
            }
            gfxTbl.insert(v.first, t);
        }
        out.insert("gfx", gfxTbl);

        toml::table OXRTbl;
        OXRTbl.insert("worldScale", world_scale);
        out.insert("OpenXR", OXRTbl);

        f << out;
        f.close();
        return f.good();
    }

    static Config fromToml(const std::filesystem::path& path)
    {
        toml::table parsed;
        auto cfg = Config {};

        if (!std::filesystem::exists(path)) {
            if (!cfg.Write(path)) {
                MessageBoxA(nullptr, "Could not write openRBRVR.toml", "Error", MB_OK);
            }
            return cfg;
        } else {
            try {
                parsed = toml::parse_file(path.c_str());
            } catch (const toml::parse_error& e) {
                MessageBoxA(nullptr, std::format("Failed to parse openRBRVR.toml: {}. Please check the syntax.", e.what()).c_str(), "Parse error", MB_OK);
                return cfg;
            }
        }
        if (parsed.size() == 0) {
            MessageBoxA(nullptr, "openRBRVR.toml is empty, continuing with default config.", "Parse error", MB_OK);
            return cfg;
        }
        cfg.menu_size = parsed["menuSize"].value_or(1.0f);
        cfg.overlay_size = parsed["overlaySize"].value_or(1.0f);
        cfg.overlay_translation.x = parsed["overlayTranslateX"].value_or(0.0f);
        cfg.overlay_translation.y = parsed["overlayTranslateY"].value_or(0.0f);
        cfg.overlay_translation.z = parsed["overlayTranslateZ"].value_or(0.0f);
        cfg.lock_to_horizon = static_cast<HorizonLock>(parsed["lockToHorizon"].value_or(0));
        cfg.horizon_lock_multiplier = parsed["horizonLockMultiplier"].value_or(1.0);
        cfg.companion_mode = companion_mode_from_str(parsed["desktopWindowMode"].value_or("vreye"));
        cfg.draw_loading_screen = parsed["drawLoadingScreen"].value_or(true);
        cfg.debug = parsed["debug"].value_or(false);
        cfg.debug_mode = parsed["debugMode"].value_or(0);
        cfg.render_mainmenu_3d = parsed["renderMainMenu3d"].value_or(false);
        cfg.render_pausemenu_3d = parsed["renderPauseMenu3d"].value_or(true);
        cfg.render_prestage_3d = parsed["renderPreStage3d"].value_or(false);
        cfg.render_replays_3d = parsed["renderReplays3d"].value_or(false);
        cfg.companion_offset = { parsed["desktopWindowOffsetX"].value_or(0), parsed["desktopWindowOffsetY"].value_or(0) };
        cfg.companion_size = parsed["desktopWindowSize"].value_or(100);
        cfg.companion_eye = static_cast<RenderTarget>(std::clamp(parsed["desktopEye"].value_or(0), 0, 1));

        const std::string& runtime = parsed["runtime"].value_or("steamvr");
        if (runtime == "openxr") {
            cfg.runtime = OPENXR;
            cfg.wmr_workaround = false;
        } else if (runtime == "openxr-wmr") {
            cfg.runtime = OPENXR;
            cfg.wmr_workaround = true;
        } else {
            cfg.runtime = OPENVR;
        }

        auto gfxnode = parsed["gfx"];
        if (gfxnode.is_table()) {
            toml::table* gfx = gfxnode.as_table();
            gfx->for_each([&cfg](const toml::key& key, toml::table& val) {
                auto k = std::string(key.data());
                const auto ss = val["superSampling"].value_or(1.0f);
                const auto stagesNode = val["stages"];
                std::vector<int> stages;
                if (stagesNode.is_array()) {
                    stagesNode.as_array()->for_each([&stages](toml::value<int64_t>& v) {
                        stages.push_back(static_cast<int>(*v));
                    });
                }
                cfg.gfx[k] = std::make_tuple(ss, stages);
            });
        }

        auto oxrnode = parsed["OpenXR"];
        if (oxrnode.is_table()) {
            cfg.world_scale = std::clamp(oxrnode["worldScale"].value_or(1000), 500, 1500);
        }

        return cfg;
    }

    void applyDxvkConf()
    {
        std::ifstream dxvkConfig("dxvk.conf");
        std::string line;
        if (dxvkConfig.good()) {
            while (std::getline(dxvkConfig, line)) {
                auto end = std::remove_if(line.begin(), line.end(), isspace);
                auto s = std::string(line.begin(), end);
                if (s.starts_with("d3d9.forceSwapchainMSAA")) {
                    std::stringstream ss { s };
                    std::string key, value;
                    std::getline(ss, key, '=');
                    std::getline(ss, value);
                    msaa = std::max<D3DMULTISAMPLE_TYPE>(D3DMULTISAMPLE_NONE, static_cast<D3DMULTISAMPLE_TYPE>(int_or_default(value, 0)));
                }
                if (s.starts_with("d3d9.samplerAnisotropy")) {
                    std::stringstream ss { s };
                    std::string key, value;
                    std::getline(ss, key, '=');
                    std::getline(ss, value);
                    anisotropy = std::max<int>(int_or_default(value, 0), 0);
                }
            }
        } else {
            msaa = D3DMULTISAMPLE_NONE;
        }
    }

    static Config fromPath(const std::filesystem::path& path)
    {
        auto tomlCfg = fromToml(path / "openRBRVR.toml");
        tomlCfg.applyDxvkConf();

        return tomlCfg;
    }
};
