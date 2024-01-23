#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <d3d9.h>

#include "Util.hpp"

#include <vec3.hpp>

#define TOML_HEADER_ONLY 1
#include <toml.hpp>

enum VRRuntime {
    OPENVR = 1,
    OPENXR = 2,
};

static float floatOrDefault(const std::string& value, float def)
{
    try {
        return std::stof(value);
    } catch (const std::exception&) {
        return def;
    }
}

static int intOrDefault(const std::string& value, int def)
{
    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        return def;
    }
}

struct Config {
    enum HorizonLock : uint8_t {
        LOCK_NONE = 0x0,
        LOCK_ROLL = 0x1,
        LOCK_PITCH = 0x2,
    };

    float menuSize = 1.0;
    float overlaySize = 1.0;
    glm::vec3 overlayTranslation = { 0, 0, 0 };
    float superSampling = 1.0;
    HorizonLock lockToHorizon = HorizonLock::LOCK_NONE;
    float horizonLockMultiplier = 1.0;
    bool drawCompanionWindow = true;
    bool drawLoadingScreen = true;
    bool debug = false;
    bool renderMainMenu3d = false;
    bool renderPauseMenu3d = true;
    bool renderPreStage3d = false;
    bool renderReplays3d = false;
    D3DMULTISAMPLE_TYPE msaa = D3DMULTISAMPLE_NONE;
    int anisotropy = 0;
    bool wmrWorkaround = false;
    VRRuntime runtime = OPENVR;

    auto operator<=>(const Config&) const = default;

    bool Write(const std::filesystem::path& path) const
    {
        std::ofstream f(path);
        if (!f.good()) {
            return false;
        }
        toml::table out {
            { "superSampling", superSampling },
            { "menuSize", menuSize },
            { "overlaySize", overlaySize },
            { "overlayTranslateX", overlayTranslation.x },
            { "overlayTranslateY", overlayTranslation.y },
            { "overlayTranslateZ", overlayTranslation.z },
            { "lockToHorizon", static_cast<int>(lockToHorizon) },
            { "horizonLockMultiplier", horizonLockMultiplier },
            { "drawDesktopWindow", drawCompanionWindow },
            { "drawLoadingScreen", drawLoadingScreen },
            { "debug", debug },
            { "renderMainMenu3d", renderMainMenu3d },
            { "renderPauseMenu3d", renderPauseMenu3d },
            { "renderPreStage3d", renderPreStage3d },
            { "renderReplays3d", renderReplays3d },
            { "runtime", runtime == OPENVR ? "steamvr" : (wmrWorkaround ? "openxr-wmr" : "openxr") },
        };
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
            }
        }

        cfg.menuSize = parsed.get("menuSize")->as_floating_point()->value_or(1.0f);
        cfg.overlaySize = parsed.get("overlaySize")->as_floating_point()->value_or(1.0f);
        cfg.overlayTranslation.x = parsed.get("overlayTranslateX")->as_floating_point()->value_or(0.0f);
        cfg.overlayTranslation.y = parsed.get("overlayTranslateY")->as_floating_point()->value_or(0.0f);
        cfg.overlayTranslation.z = parsed.get("overlayTranslateZ")->as_floating_point()->value_or(0.0f);
        cfg.superSampling = parsed.get("superSampling")->as_floating_point()->value_or(1.0f);
        cfg.lockToHorizon = static_cast<HorizonLock>(parsed.get("lockToHorizon")->as_integer()->value_or(0));
        cfg.horizonLockMultiplier = parsed.get("horizonLockMultiplier")->as_floating_point()->value_or(1.0f);
        cfg.drawCompanionWindow = parsed.get("drawDesktopWindow")->as_boolean()->value_or(true);
        cfg.drawLoadingScreen = parsed.get("drawLoadingScreen")->as_boolean()->value_or(true);
        cfg.debug = parsed.get("debug")->as_boolean()->value_or(false);
        cfg.renderMainMenu3d = parsed.get("renderMainMenu3d")->as_boolean()->value_or(false);
        cfg.renderPauseMenu3d = parsed.get("renderPauseMenu3d")->as_boolean()->value_or(true);
        cfg.renderPreStage3d = parsed.get("renderPreStage3d")->as_boolean()->value_or(false);
        cfg.renderReplays3d = parsed.get("renderReplays3d")->as_boolean()->value_or(false);
        const std::string& runtime = parsed.get("runtime")->as_string()->value_or("steamvr");
        if (runtime == "openxr") {
            cfg.runtime = OPENXR;
            cfg.wmrWorkaround = false;
        } else if (runtime == "openxr-wmr") {
            cfg.runtime = OPENXR;
            cfg.wmrWorkaround = true;
        } else {
            cfg.runtime = OPENVR;
        }
    }

    static std::optional<Config> fromIni(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path)) {
            return std::nullopt;
        }
        Config cfg = {};

        std::ifstream f(path);
        if (!f.good()) {
            Dbg("Could not open openRBRVR.ini");
            return std::nullopt;
        }

        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) {
                continue;
            }
            if (line.empty() || line.front() == ';') {
                // Skip empty lines and comments
                continue;
            }

            auto end = std::remove_if(line.begin(), line.end(), isspace);
            auto s = std::string(line.begin(), end);
            std::stringstream ss { s };

            std::string key, value;
            std::getline(ss, key, '=');
            std::getline(ss, value);

            if (key == "menuSize") {
                cfg.menuSize = floatOrDefault(value, 1.0);
            } else if (key == "overlaySize") {
                cfg.overlaySize = floatOrDefault(value, 1.0);
            } else if (key == "overlayTranslateX") {
                cfg.overlayTranslation.x = floatOrDefault(value, 1.0);
            } else if (key == "overlayTranslateY") {
                cfg.overlayTranslation.y = floatOrDefault(value, 1.0);
            } else if (key == "overlayTranslateZ") {
                cfg.overlayTranslation.z = floatOrDefault(value, 1.0);
            } else if (key == "superSampling") {
                cfg.superSampling = floatOrDefault(value, 1.0);
            } else if (key == "lockToHorizon") {
                cfg.lockToHorizon = static_cast<HorizonLock>(intOrDefault(value, 0));
            } else if (key == "horizonLockMultiplier") {
                cfg.horizonLockMultiplier = floatOrDefault(value, 1.0);
            } else if (key == "drawDesktopWindow") {
                cfg.drawCompanionWindow = (value == "true");
            } else if (key == "drawLoadingScreen") {
                cfg.drawLoadingScreen = (value == "true");
            } else if (key == "debug") {
                cfg.debug = (value == "true");
            } else if (key == "renderMainMenu3d") {
                cfg.renderMainMenu3d = (value == "true");
            } else if (key == "renderPauseMenu3d") {
                cfg.renderPauseMenu3d = (value == "true");
            } else if (key == "renderPreStage3d") {
                cfg.renderPreStage3d = (value == "true");
            } else if (key == "renderReplays3d") {
                cfg.renderReplays3d = (value == "true");
            } else if (key == "wmrWorkaround") {
                cfg.wmrWorkaround = (value == "true");
            } else if (key == "runtime") {
                if (value == "openxr") {
                    cfg.runtime = OPENXR;
                    cfg.wmrWorkaround = false;
                } else if (value == "openxrwmr") {
                    cfg.runtime = OPENXR;
                    cfg.wmrWorkaround = true;
                } else {
                    cfg.runtime = OPENVR;
                }
            }
        }

        return cfg;
    }

    void applyIniFields(const Config& iniCfg)
    {
        menuSize = iniCfg.menuSize;
        overlaySize = iniCfg.overlaySize;
        overlayTranslation = iniCfg.overlayTranslation;
        superSampling = iniCfg.superSampling;
        lockToHorizon = iniCfg.lockToHorizon;
        horizonLockMultiplier = iniCfg.horizonLockMultiplier;
        drawCompanionWindow = iniCfg.drawCompanionWindow;
        drawLoadingScreen = iniCfg.drawLoadingScreen;
        debug = iniCfg.debug;
        renderMainMenu3d = iniCfg.renderMainMenu3d;
        renderPauseMenu3d = iniCfg.renderPauseMenu3d;
        renderPreStage3d = iniCfg.renderPreStage3d;
        renderReplays3d = iniCfg.renderReplays3d;
        wmrWorkaround = iniCfg.wmrWorkaround;
        runtime = iniCfg.runtime;
    }

    static void parseDxvkConf(Config& cfg)
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
                    cfg.msaa = std::max<D3DMULTISAMPLE_TYPE>(D3DMULTISAMPLE_NONE, static_cast<D3DMULTISAMPLE_TYPE>(intOrDefault(value, 0)));
                }
                if (s.starts_with("d3d9.samplerAnisotropy")) {
                    std::stringstream ss { s };
                    std::string key, value;
                    std::getline(ss, key, '=');
                    std::getline(ss, value);
                    cfg.anisotropy = std::max<int>(intOrDefault(value, 0), 0);
                }
            }
        } else {
            cfg.msaa = D3DMULTISAMPLE_NONE;
        }
    }

    static Config fromPath(const std::filesystem::path& path)
    {
        auto iniPath = path / "openRBRVR.ini";
        auto tomlPath = path / "openRBRVR.toml";

        auto tomlCfg = fromToml(tomlPath);

        auto iniModifyTime = std::filesystem::last_write_time(iniPath);
        auto tomlModifyTime = std::filesystem::last_write_time(tomlPath);

        if (iniModifyTime > tomlModifyTime) {
            auto iniCfg = fromIni(iniPath);
            if (iniCfg) {
                tomlCfg.applyIniFields(iniCfg.value());
                tomlCfg.Write(tomlPath);
            }
        }

        return tomlCfg;
    }
};
