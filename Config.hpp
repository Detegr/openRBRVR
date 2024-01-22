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
#include "glm/vec3.hpp"

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

// Simplified ini file parser for plugin configuration
struct Config {
    enum HorizonLock : uint8_t {
        LOCK_NONE = 0x0,
        LOCK_ROLL = 0x1,
        LOCK_PITCH = 0x2,
    };

    float menuSize;
    float overlaySize;
    glm::vec3 overlayTranslation;
    float superSampling;
    HorizonLock lockToHorizon;
    float horizonLockMultiplier;
    bool drawCompanionWindow;
    bool drawLoadingScreen;
    bool debug;
    bool renderMainMenu3d;
    bool renderPauseMenu3d;
    bool renderPreStage3d;
    bool renderReplays3d;
    D3DMULTISAMPLE_TYPE msaa;
    int anisotropy;
    bool wmrWorkaround;
    VRRuntime runtime;

    auto operator<=>(const Config&) const = default;

    std::string ToString() const
    {
        return std::format(
            "superSampling = {:.2f}\n"
            "menuSize = {:.2f}\n"
            "overlaySize = {:.2f}\n"
            "overlayTranslateX = {:.2f}\n"
            "overlayTranslateY = {:.2f}\n"
            "overlayTranslateZ = {:.2f}\n"
            "lockToHorizon = {}\n"
            "horizonLockMultiplier = {}\n"
            "drawDesktopWindow = {}\n"
            "drawLoadingScreen = {}\n"
            "debug = {}\n"
            "renderMainMenu3d = {}\n"
            "renderPauseMenu3d = {}\n"
            "renderPreStage3d = {}\n"
            "renderReplays3d = {}\n"
            "runtime = {}",
            superSampling,
            menuSize,
            overlaySize,
            overlayTranslation.x,
            overlayTranslation.y,
            overlayTranslation.z,
            static_cast<int>(lockToHorizon),
            horizonLockMultiplier,
            drawCompanionWindow,
            drawLoadingScreen,
            debug,
            renderMainMenu3d,
            renderPauseMenu3d,
            renderPreStage3d,
            renderReplays3d,
            runtime == OPENVR ? "steamvr" : (wmrWorkaround ? "openxr-wmr" : "openxr"));
    }

    bool Write(const std::filesystem::path& path) const
    {
        std::ofstream f(path);
        if (!f.good()) {
            return false;
        }
        f << ToString();
        f.close();
        return f.good();
    }

    static Config fromFile(const std::filesystem::path& path)
    {
        auto cfg = Config {
            .menuSize = 1.0,
            .overlaySize = 1.0,
            .overlayTranslation = glm::vec3 { 0.0f, 0.0f, 0.0f },
            .superSampling = 1.0,
            .lockToHorizon = LOCK_NONE,
            .horizonLockMultiplier = 1.0,
            .drawCompanionWindow = true,
            .drawLoadingScreen = true,
            .debug = false,
            .renderMainMenu3d = false,
            .renderPauseMenu3d = true,
            .renderPreStage3d = false,
            .renderReplays3d = false,
            .msaa = D3DMULTISAMPLE_NONE,
            .anisotropy = -1,
            .wmrWorkaround = false,
            .runtime = OPENVR,
        };

        if (!std::filesystem::exists(path)) {
            cfg.Write(path);
            return cfg;
        }

        std::ifstream f(path);
        if (!f.good()) {
            Dbg("Could not open openRBRVR.ini");
            return cfg;
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
                } else if (value == "openxr-wmr") {
                    cfg.runtime = OPENXR;
                    cfg.wmrWorkaround = true;
                } else {
                    cfg.runtime = OPENVR;
                }
            }
        }

        std::ifstream dxvkConfig("dxvk.conf");
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
        return cfg;
    }
};
