#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "Util.hpp"
#include "glm/vec3.hpp"

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
    bool drawCompanionWindow;
    bool drawLoadingScreen;
    bool debug;

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
            "drawDesktopWindow = {}\n"
            "drawLoadingScreen = {}\n"
            "debug = {}",
            superSampling,
            menuSize,
            overlaySize,
            overlayTranslation.x,
            overlayTranslation.y,
            overlayTranslation.z,
            (int)lockToHorizon,
            drawCompanionWindow,
            drawLoadingScreen,
            debug);
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
            .drawCompanionWindow = true,
            .drawLoadingScreen = true,
            .debug = false,
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
            } else if (key == "drawDesktopWindow") {
                cfg.drawCompanionWindow = (value == "true");
            } else if (key == "drawLoadingScreen") {
                cfg.drawLoadingScreen = (value == "true");
            } else if (key == "debug") {
                cfg.debug = (value == "true");
            }
        }
        return cfg;
    }
};
