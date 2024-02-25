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
    int debugMode = 0;
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
        menuSize = rhs.menuSize;
        overlaySize = rhs.overlaySize;
        overlayTranslation = rhs.overlayTranslation;
        superSampling = rhs.superSampling;
        lockToHorizon = rhs.lockToHorizon;
        horizonLockMultiplier = rhs.horizonLockMultiplier;
        companionMode = rhs.companionMode;
        drawLoadingScreen = rhs.drawLoadingScreen;
        debug = rhs.debug;
        renderMainMenu3d = rhs.renderMainMenu3d;
        renderPauseMenu3d = rhs.renderPauseMenu3d;
        renderPreStage3d = rhs.renderPreStage3d;
        renderReplays3d = rhs.renderReplays3d;
        wmrWorkaround = rhs.wmrWorkaround;
        runtime = rhs.runtime;
        msaa = rhs.msaa;
        anisotropy = rhs.anisotropy;
        gfx = rhs.gfx;
        companionOffset = rhs.companionOffset;
        companionSize = rhs.companionSize;
        companionEye = rhs.companionEye;
        companionMode = rhs.companionMode;
        debugMode = rhs.debugMode;
        worldScale = rhs.worldScale;
        return *this;
    }

    bool operator==(const Config& rhs) const
    {
        return menuSize == rhs.menuSize
            && overlaySize == rhs.overlaySize
            && overlayTranslation == rhs.overlayTranslation
            && superSampling == rhs.superSampling
            && lockToHorizon == rhs.lockToHorizon
            && horizonLockMultiplier == rhs.horizonLockMultiplier
            && companionMode == rhs.companionMode
            && drawLoadingScreen == rhs.drawLoadingScreen
            && debug == rhs.debug
            && debugMode == rhs.debugMode
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
            { "debugMode", debugMode },
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
        cfg.debugMode = parsed["debugMode"].value_or(0);
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
        auto tomlCfg = fromToml(path / "openRBRVR.toml");
        tomlCfg.applyDxvkConf();

        return tomlCfg;
    }
};
