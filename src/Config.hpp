#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <unordered_map>

#include <d3d9.h>

#include "Util.hpp"

#include <vec2.hpp>
#include <vec3.hpp>

#define TOML_HEADER_ONLY 1
#include <inicpp.h>
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

static std::string vec3_as_space_separated_string(const glm::vec3& vec)
{
    auto v = glm::value_ptr(vec);
    return std::format("{:.4f} {:.4f} {:.4f}", v[0], v[1], v[2]);
}

static std::optional<glm::vec3> vec3_from_space_separated_string(const std::string& s)
{
    std::string val;
    std::stringstream ss(s);

    int i = 0;
    auto ret = glm::vec3 {};
    auto v = glm::value_ptr(ret);
    while (std::getline(ss, val, ' ')) {
        auto idx = i;
        i++;

        if (val.size() == 0) {
            continue;
        }

        if (idx >= 3) {
            return ret;
        }

        float f {};
        auto [p, ec] = std::from_chars(val.data(), val.data() + val.size(), f);
        if (ec != std::errc()) {
            return std::nullopt;
        }

        v[idx] = f;
    }

    if (i < 2) {
        return std::nullopt;
    }

    return ret;
}

struct RenderContextConfig {
    float supersampling = 1.0;
    std::optional<D3DMULTISAMPLE_TYPE> msaa = std::nullopt;
    std::vector<int> stage_ids = {};
};

struct Config {
    float menu_size = 1.0;
    float overlay_size = 1.0;
    glm::vec3 overlay_translation = { 0, 0, 0 };
    float supersampling = 1.0;
    HorizonLock lock_to_horizon = HorizonLock::LOCK_NONE;
    double horizon_lock_multiplier = 1.0;
    bool horizon_lock_flip = false;
    CompanionMode companion_mode;
    bool draw_loading_screen = true;
    bool debug = false;
    int debug_mode = 0;
    bool render_mainmenu_3d = false;
    bool render_pausemenu_3d = true;
    bool render_prestage_3d = false;
    bool render_replays_3d = false;
    int anisotropy = 0;
    bool wmr_workaround = false;
    VRRuntime runtime = VRRuntime::OPENVR;
    std::unordered_map<std::string, RenderContextConfig> gfx = { { "default", RenderContextConfig {} } };
    glm::ivec2 companion_offset;
    int companion_size = 100;
    RenderTarget companion_eye = LeftEye;
    int world_scale = 1000;
    bool legacy_openxr_init = false;

    Config& operator=(const Config& rhs)
    {
        menu_size = rhs.menu_size;
        overlay_size = rhs.overlay_size;
        overlay_translation = rhs.overlay_translation;
        supersampling = rhs.supersampling;
        lock_to_horizon = rhs.lock_to_horizon;
        horizon_lock_multiplier = rhs.horizon_lock_multiplier;
        horizon_lock_flip = rhs.horizon_lock_flip;
        companion_mode = rhs.companion_mode;
        draw_loading_screen = rhs.draw_loading_screen;
        debug = rhs.debug;
        render_mainmenu_3d = rhs.render_mainmenu_3d;
        render_pausemenu_3d = rhs.render_pausemenu_3d;
        render_prestage_3d = rhs.render_prestage_3d;
        render_replays_3d = rhs.render_replays_3d;
        wmr_workaround = rhs.wmr_workaround;
        runtime = rhs.runtime;
        anisotropy = rhs.anisotropy;
        gfx = rhs.gfx;
        companion_offset = rhs.companion_offset;
        companion_size = rhs.companion_size;
        companion_eye = rhs.companion_eye;
        companion_mode = rhs.companion_mode;
        debug_mode = rhs.debug_mode;
        world_scale = rhs.world_scale;
        legacy_openxr_init = rhs.legacy_openxr_init;
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
            && horizon_lock_flip == rhs.horizon_lock_flip
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
            && world_scale == rhs.world_scale
            && legacy_openxr_init == rhs.legacy_openxr_init;
    }

    bool write(const std::filesystem::path& path) const
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
            { "horizonLockFlip", horizon_lock_flip },
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

            const auto& ctx = v.second;
            for (const auto& stage : ctx.stage_ids) {
                a.push_back(stage);
            }

            t.insert("superSampling", round(ctx.supersampling));
            if (ctx.msaa) {
                t.insert("antiAliasing", static_cast<int>(ctx.msaa.value()));
            }
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

    static Config from_toml(const std::filesystem::path& path)
    {
        toml::table parsed;
        auto cfg = Config {};

        if (!std::filesystem::exists(path)) {
            if (!cfg.write(path)) {
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
        cfg.horizon_lock_flip = parsed["horizonLockFlip"].value_or(false);
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

        // Use legacy OpenXR init if WMR workaround is enabled
        // WMR workaround isn't compatible with XR_KHR_vulkan_enable2
        // However, WMR devices *maybe* work with XR_KHR_vulkan_enable2 without the workaround, we'll see
        cfg.legacy_openxr_init = parsed["legacyOpenXRInit"].value_or(cfg.wmr_workaround);

        auto gfxnode = parsed["gfx"];
        if (gfxnode.is_table()) {
            toml::table* gfx = gfxnode.as_table();
            gfx->for_each([&cfg](const toml::key& key, toml::table& val) {
                auto k = std::string(key.data());
                auto ss = val["superSampling"].value_or(1.0f);
                auto msaa_int = val["antiAliasing"];
                std::optional<D3DMULTISAMPLE_TYPE> msaa;
                if (msaa_int) {
                    msaa = static_cast<D3DMULTISAMPLE_TYPE>(msaa_int.value_or(0));
                }
                const auto stagesNode = val["stages"];
                std::vector<int> stages;
                if (stagesNode.is_array()) {
                    stagesNode.as_array()->for_each([&stages](toml::value<int64_t>& v) {
                        stages.push_back(static_cast<int>(*v));
                    });
                }
                cfg.gfx[k] = RenderContextConfig { ss, msaa, stages };
            });
        }

        auto oxrnode = parsed["OpenXR"];
        if (oxrnode.is_table()) {
            cfg.world_scale = std::clamp(oxrnode["worldScale"].value_or(1000), 500, 1500);
        }

        return cfg;
    }

    void apply_dxvk_conf()
    {
        std::ifstream dxvk_config("dxvk.conf");
        std::string line;
        if (dxvk_config.good()) {
            while (std::getline(dxvk_config, line)) {
                auto end = std::remove_if(line.begin(), line.end(), isspace);
                auto s = std::string(line.begin(), end);
                if (s.starts_with("d3d9.forceSwapchainMSAA")) {
                    std::stringstream ss { s };
                    std::string key, value;
                    std::getline(ss, key, '=');
                    std::getline(ss, value);
                    gfx["default"].msaa = std::max<D3DMULTISAMPLE_TYPE>(D3DMULTISAMPLE_NONE, static_cast<D3DMULTISAMPLE_TYPE>(int_or_default(value, 0)));
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
            gfx["default"].msaa = D3DMULTISAMPLE_NONE;
        }
    }

    static std::optional<std::string> to_string(const std::filesystem::path& p)
    {
        return p.generic_string();
    }

    static std::optional<std::filesystem::path> resolve_car_ini_path(uint32_t car_id)
    {
        auto cars_ini_path = "Cars\\cars.ini";
        if (!std::filesystem::exists(cars_ini_path)) {
            dbg("Could not resolve car ini path");
            return std::nullopt;
        }

        try {
            ini::IniFile cars_ini(cars_ini_path);
            auto car_key = std::format("Car0{}", car_id);
            return std::filesystem::path(cars_ini[car_key]["IniFile"].as<std::string>()
                | std::ranges::views::filter([](char c) { return c != '"'; })
                | std::ranges::to<std::string>());
        } catch (...) {
            dbg("Could not resolve car ini path");
            return std::nullopt;
        }
    }

    static std::optional<std::filesystem::path> resolve_personal_car_ini_path(uint32_t car_id)
    {
        auto ini_file_path = resolve_car_ini_path(car_id);
        if (!ini_file_path) {
            return std::nullopt;
        }

        auto personal_filename = ini_file_path.value().filename();
        personal_filename.replace_extension("");
        personal_filename += "_personal";
        personal_filename.replace_extension(".ini");
        ini_file_path.value().replace_filename(personal_filename);

        return ini_file_path;
    }

    static bool insert_or_update_seat_translation(uint32_t car_id, const glm::vec3& seat_translation)
    {
        auto ini_path = resolve_personal_car_ini_path(car_id).and_then(to_string);
        if (!ini_path) {
            return false;
        }

        try {
            ini::IniFile personal_ini(ini_path.value());
            personal_ini["openRBRVR"]["seatPosition"] = vec3_as_space_separated_string(seat_translation);
            personal_ini.save(ini_path.value());
        } catch (...) {
            dbg("Updating seat translation failed");
            return false;
        }

        return true;
    }

    std::tuple<glm::vec3, bool> load_seat_translation(uint32_t car_id)
    {
        const auto default_translation = std::make_tuple(glm::vec3 { 0.33, 1.0, -1.0 }, true);
        auto ini_path = resolve_personal_car_ini_path(car_id).and_then(to_string);
        if (!ini_path) {
            return default_translation;
        }

        bool is_openrbrvr_translation = false;
        std::optional<glm::vec3> seat_translation = std::nullopt;

        try {
            if (std::filesystem::exists(ini_path.value())) {
                ini::IniFile personal_ini(ini_path.value());

                seat_translation = vec3_from_space_separated_string(personal_ini["openRBRVR"]["seatPosition"].as<std::string>());

                if (!seat_translation) {
                    seat_translation = vec3_from_space_separated_string(personal_ini["Cam_internal"]["Pos"].as<std::string>());
                } else {
                    is_openrbrvr_translation = true;
                }
            }

            if (!seat_translation) {
                ini_path = resolve_car_ini_path(car_id).and_then(to_string);
                if (!ini_path) {
                    return default_translation;
                }
                ini::IniFile ini(ini_path.value());
                seat_translation = vec3_from_space_separated_string(ini["Cam_internal"]["Pos"].as<std::string>());
            }

            return seat_translation
                .and_then([&](const glm::vec3& t) { return std::optional(std::make_tuple(t, is_openrbrvr_translation)); })
                .value_or(default_translation);

        } catch (...) {
            dbg("Loading seat translation failed");
            return default_translation;
        }
    }

    static Config from_path(const std::filesystem::path& path)
    {
        auto toml_cfg = from_toml(path / "openRBRVR.toml");
        toml_cfg.apply_dxvk_conf();

        return toml_cfg;
    }
};
