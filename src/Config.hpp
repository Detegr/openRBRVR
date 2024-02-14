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
    float menuSize = 1.0;
    float overlaySize = 1.0;
    glm::vec3 overlayTranslation = { 0, 0, 0 };
    float superSampling = 1.0;
    HorizonLock lockToHorizon = HorizonLock::LOCK_NONE;
    double horizonLockMultiplier = 1.0;
    CompanionMode companionMode;
    bool drawLoadingScreen = true;
    bool debug = false;
    bool renderMainMenu3d = false;
    bool renderPauseMenu3d = true;
    bool renderPreStage3d = false;
    bool renderReplays3d = false;
    D3DMULTISAMPLE_TYPE msaa = D3DMULTISAMPLE_NONE;
    int anisotropy = 0;
    bool wmrWorkaround = false;
    VRRuntime runtime = VRRuntime::OPENVR;
    std::unordered_map<std::string, std::tuple<float, std::vector<int>>> gfx = { { "default", std::make_tuple(1.0f, std::vector<int> {}) } };
    glm::ivec2 companionOffset;
    int companionSize = 100;
    RenderTarget companionEye = LeftEye;
    int worldScale = 1000;

    Config& operator=(const Config& rhs)
    {
        applyIniFields(rhs);
        msaa = rhs.msaa;
        anisotropy = rhs.anisotropy;
        gfx = rhs.gfx;
        companionOffset = rhs.companionOffset;
        companionSize = rhs.companionSize;
        companionEye = rhs.companionEye;
        companionMode = rhs.companionMode;
        worldScale = rhs.worldScale;
        return *this;
    }

    bool operator==(const Config& rhs) const
    {
        // We need to compare ini fields only as gfx is not modifiable through the game
        return menuSize == rhs.menuSize
            && overlaySize == rhs.overlaySize
            && overlayTranslation == rhs.overlayTranslation
            && superSampling == rhs.superSampling
            && lockToHorizon == rhs.lockToHorizon
            && horizonLockMultiplier == rhs.horizonLockMultiplier
            && companionMode == rhs.companionMode
            && drawLoadingScreen == rhs.drawLoadingScreen
            && debug == rhs.debug
            && renderMainMenu3d == rhs.renderMainMenu3d
            && renderPauseMenu3d == rhs.renderPauseMenu3d
            && renderPreStage3d == rhs.renderPreStage3d
            && renderReplays3d == rhs.renderReplays3d
            && wmrWorkaround == rhs.wmrWorkaround
            && runtime == rhs.runtime
            && companionOffset == rhs.companionOffset
            && companionSize == rhs.companionSize
            && companionEye == rhs.companionEye
            && worldScale == rhs.worldScale;
    }

    bool Write(const std::filesystem::path& path) const
    {
        constexpr auto round = [](double v) -> double { return std::round(v * 100.0) / 100.0; };

        std::ofstream f(path);
        if (!f.good()) {
            return false;
        }
        toml::table out {
            { "menuSize", round(menuSize) },
            { "overlaySize", round(overlaySize) },
            { "overlayTranslateX", round(overlayTranslation.x) },
            { "overlayTranslateY", round(overlayTranslation.y) },
            { "overlayTranslateZ", round(overlayTranslation.z) },
            { "lockToHorizon", static_cast<int>(lockToHorizon) },
            { "horizonLockMultiplier", round(horizonLockMultiplier) },
            { "desktopWindowMode", CompanionModeStr(companionMode) },
            { "drawLoadingScreen", drawLoadingScreen },
            { "debug", debug },
            { "renderMainMenu3d", renderMainMenu3d },
            { "renderPauseMenu3d", renderPauseMenu3d },
            { "renderPreStage3d", renderPreStage3d },
            { "renderReplays3d", renderReplays3d },
            { "runtime", runtime == OPENVR ? "steamvr" : (wmrWorkaround ? "openxr-wmr" : "openxr") },
            { "desktopWindowOffsetX", companionOffset.x },
            { "desktopWindowOffsetY", companionOffset.y },
            { "desktopWindowSize", companionSize },
            { "desktopEye", static_cast<int>(companionEye) },
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
        OXRTbl.insert("worldScale", worldScale);
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
        cfg.menuSize = parsed["menuSize"].value_or(1.0f);
        cfg.overlaySize = parsed["overlaySize"].value_or(1.0f);
        cfg.overlayTranslation.x = parsed["overlayTranslateX"].value_or(0.0f);
        cfg.overlayTranslation.y = parsed["overlayTranslateY"].value_or(0.0f);
        cfg.overlayTranslation.z = parsed["overlayTranslateZ"].value_or(0.0f);
        cfg.lockToHorizon = static_cast<HorizonLock>(parsed["lockToHorizon"].value_or(0));
        cfg.horizonLockMultiplier = parsed["horizonLockMultiplier"].value_or(1.0);
        cfg.companionMode = CompanionModeFromStr(parsed["desktopWindowMode"].value_or("vreye"));
        cfg.drawLoadingScreen = parsed["drawLoadingScreen"].value_or(true);
        cfg.debug = parsed["debug"].value_or(false);
        cfg.renderMainMenu3d = parsed["renderMainMenu3d"].value_or(false);
        cfg.renderPauseMenu3d = parsed["renderPauseMenu3d"].value_or(true);
        cfg.renderPreStage3d = parsed["renderPreStage3d"].value_or(false);
        cfg.renderReplays3d = parsed["renderReplays3d"].value_or(false);
        cfg.companionOffset = { parsed["desktopWindowOffsetX"].value_or(0), parsed["desktopWindowOffsetY"].value_or(0) };
        cfg.companionSize = parsed["desktopWindowSize"].value_or(100);
        cfg.companionEye = static_cast<RenderTarget>(std::clamp(parsed["desktopEye"].value_or(0), 0, 1));

        const std::string& runtime = parsed["runtime"].value_or("steamvr");
        if (runtime == "openxr") {
            cfg.runtime = OPENXR;
            cfg.wmrWorkaround = false;
        } else if (runtime == "openxr-wmr") {
            cfg.runtime = OPENXR;
            cfg.wmrWorkaround = true;
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
            cfg.worldScale = std::clamp(oxrnode["worldScale"].value_or(1000), 500, 1500);
        }

        return cfg;
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
                cfg.companionMode = (value == "true") ? CompanionMode::VREye : CompanionMode::Off;
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
        companionMode = iniCfg.companionMode;
        drawLoadingScreen = iniCfg.drawLoadingScreen;
        debug = iniCfg.debug;
        renderMainMenu3d = iniCfg.renderMainMenu3d;
        renderPauseMenu3d = iniCfg.renderPauseMenu3d;
        renderPreStage3d = iniCfg.renderPreStage3d;
        renderReplays3d = iniCfg.renderReplays3d;
        wmrWorkaround = iniCfg.wmrWorkaround;
        runtime = iniCfg.runtime;

        // Patch [gfx.default] supersampling from the ini value
        float& ss = std::get<0>(gfx["default"]);
        ss = iniCfg.superSampling;
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
                    msaa = std::max<D3DMULTISAMPLE_TYPE>(D3DMULTISAMPLE_NONE, static_cast<D3DMULTISAMPLE_TYPE>(intOrDefault(value, 0)));
                }
                if (s.starts_with("d3d9.samplerAnisotropy")) {
                    std::stringstream ss { s };
                    std::string key, value;
                    std::getline(ss, key, '=');
                    std::getline(ss, value);
                    anisotropy = std::max<int>(intOrDefault(value, 0), 0);
                }
            }
        } else {
            msaa = D3DMULTISAMPLE_NONE;
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

        tomlCfg.applyDxvkConf();

        return tomlCfg;
    }
};
