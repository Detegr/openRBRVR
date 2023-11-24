#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

#include <iostream>

static float floatOrDefault(const std::string& value, float def)
{
    try {
        return std::stof(value);
    } catch (const std::exception&) {
        return def;
    }
}

// Simplified ini file parser for plugin configuration
struct Config {
    float menuSize;
    float overlaySize;
    float superSampling;

    static Config fromFile(const std::string& path)
    {
        auto cfg = Config {
            .menuSize = 1.0,
            .overlaySize = 1.0,
            .superSampling = 1.0,
        };

        std::ifstream f;
        try {
            f = std::ifstream(path);
        } catch (const std::exception&) {
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
            } else if (key == "superSampling") {
                cfg.superSampling = floatOrDefault(value, 1.0);
            }
        }
        return cfg;
    }
};
