#pragma once

#include "IPlugin.h"
#include "Licenses.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

using MenuFn = std::function<void()>;

struct MenuEntry {
    std::function<std::string()> text;
    std::vector<std::string> long_text;
    std::optional<IRBRGame::EFonts> font;
    std::optional<IRBRGame::EMenuColors> menu_color;
    std::optional<std::function<std::tuple<float, float, float, float>()>> color;
    std::optional<std::tuple<float, float>> position;
    std::optional<MenuFn> left_action;
    std::optional<MenuFn> right_action;
    std::optional<MenuFn> select_action;
    std::optional<std::function<bool()>> visible;
};

class Menu {
protected:
    std::string menu_heading;
    std::vector<MenuEntry> menu_entries;

public:
    int entry_idx;
    static constexpr auto menu_row_height = 21.0f;
    static constexpr std::tuple<float, float> menu_items_start_pos = std::make_tuple(65.0f, 70.0f);

    Menu(std::string heading, const std::vector<MenuEntry> entries)
        : entry_idx(0)
        , menu_heading(heading)
        , menu_entries(entries)
    {
    }

    virtual const char* heading() const
    {
        return menu_heading.c_str();
    }

    virtual void select()
    {
        const auto& action = menu_entries[entry_idx].select_action;
        if (action) {
            action.value()();
        }
    }

    virtual void up()
    {
        entry_idx--;
        if (entry_idx < 0) {
            entry_idx = menu_entries.size() - 1;
        }
        const auto& entry = menu_entries[entry_idx];
        if (entry.visible && !entry.visible.value()()) {
            // Entry not visible, go up again
            up();
        }
    }

    virtual void down()
    {
        entry_idx++;
        entry_idx %= menu_entries.size();
        const auto& entry = menu_entries[entry_idx];
        if (entry.visible && !entry.visible.value()()) {
            // Entry not visible, go down again
            down();
        }
    }

    virtual void left()
    {
        const auto& action = menu_entries[entry_idx].left_action;
        if (action) {
            action.value()();
        }
    }

    virtual void right()
    {
        const auto& action = menu_entries[entry_idx].right_action;
        if (action) {
            action.value()();
        }
    }

    virtual const std::vector<MenuEntry>& entries() const { return menu_entries; }

    // Return the index of the current entry, discarding hidden entries
    virtual const int index() const
    {
        int ret = 0;
        for (int i = 0; i < entry_idx; ++i) {
            const auto& entry = menu_entries[i];
            if (!entry.visible || entry.visible.value()()) {
                ret++;
            }
        }
        return ret;
    }

    virtual float row_height() const { return menu_row_height; }
};

class LicenseMenu : public Menu {
private:
    int menuScroll;

public:
    static constexpr auto licenseRowHeight = 14.0f;
    static constexpr auto licenseRowCount = 24;

    LicenseMenu();

    void up() override
    {
        menuScroll = std::max<int>(0, menuScroll - 1);
    }
    void down() override
    {
        menuScroll = std::min<int>(g::license_information.size() - licenseRowCount, menuScroll + 1);
    }
    void left() override;
    void select() override;

    const std::vector<MenuEntry>& entries() const override
    {
        static std::vector<MenuEntry> ret;
        ret.clear();
        ret.push_back({
            .text = [] { return "Press enter or left to go back"; },
            .font = IRBRGame::EFonts::FONT_SMALL,
            .menu_color = IRBRGame::EMenuColors::MENU_TEXT,
            .position = Menu::menu_items_start_pos,
        });
        ret.push_back({ [] { return ""; } });

        for (int i = menuScroll; i < std::min<int>(menuScroll + licenseRowCount, g::license_information.size()); ++i) {
            ret.push_back(menu_entries[i]);
        }
        return ret;
    }

    float row_height() const { return licenseRowHeight; }
    const int index() const { return -1; }
};
