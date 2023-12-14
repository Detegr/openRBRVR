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
    std::string longText;
    std::optional<IRBRGame::EFonts> font;
    std::optional<IRBRGame::EMenuColors> menuColor;
    std::optional<std::function<std::tuple<float, float, float, float>()>> color;
    std::optional<std::tuple<float, float>> position;
    std::optional<MenuFn> leftAction;
    std::optional<MenuFn> rightAction;
    std::optional<MenuFn> selectAction;
};

class Menu {
protected:
    int entryIdx;
    std::string heading;
    std::vector<MenuEntry> entries;

public:
    static constexpr auto rowHeight = 21.0f;
    static constexpr std::tuple<float, float> menuItemsStartPos = std::make_tuple(65.0f, 70.0f);

    Menu(std::string heading, const std::vector<MenuEntry> entries)
        : entryIdx(0)
        , heading(heading)
        , entries(entries)
    {
    }

    virtual const char* Heading() const { return heading.c_str(); }

    virtual void Select()
    {
        const auto& action = entries[entryIdx].selectAction;
        if (action) {
            action.value()();
        }
    }

    virtual void Up()
    {
        entryIdx--;
        if (entryIdx < 0) {
            entryIdx = entries.size() - 1;
        }
    }

    virtual void Down()
    {
        entryIdx++;
        entryIdx %= entries.size();
    }

    virtual void Left()
    {
        const auto& action = entries[entryIdx].leftAction;
        if (action) {
            action.value()();
        }
    }

    virtual void Right()
    {
        const auto& action = entries[entryIdx].rightAction;
        if (action) {
            action.value()();
        }
    }

    virtual const std::vector<MenuEntry>& Entries() const { return entries; }
    virtual const int Index() const { return entryIdx; }
    virtual float RowHeight() const { return rowHeight; }
};

class LicenseMenu : public Menu {
private:
    int menuScroll;

public:
    static constexpr auto licenseRowHeight = 14.0f;
    static constexpr auto licenseRowCount = 24;

    LicenseMenu();

    void Up() override
    {
        menuScroll = std::max<int>(0, menuScroll - 1);
    }
    void Down() override
    {
        menuScroll = std::min<int>(gLicenseInformation.size() - licenseRowCount, menuScroll + 1);
    }
    void Left() override;
    void Select() override;

    const std::vector<MenuEntry>& Entries() const override
    {
        static std::vector<MenuEntry> ret;
        ret.clear();
        ret.push_back({
            .text = [] { return "Press enter or left to go back"; },
            .font = IRBRGame::EFonts::FONT_SMALL,
            .menuColor = IRBRGame::EMenuColors::MENU_TEXT,
            .position = Menu::menuItemsStartPos,
        });
        ret.push_back({ [] { return ""; } });

        for (int i = menuScroll; i < std::min<int>(menuScroll + licenseRowCount, gLicenseInformation.size()); ++i) {
            ret.push_back(entries[i]);
        }
        return ret;
    }

    float RowHeight() const { return licenseRowHeight; }
    const int Index() const { return -1; }
};

extern class Menu* gMenu;