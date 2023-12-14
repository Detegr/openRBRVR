#include "Menu.hpp"
#include "Config.hpp"
#include "openvr/openvr.h"

#include <array>
#include <format>

extern Config gCfg;
extern Config gSavedCfg;

// Helper function to create an identity function to return its argument
template <typename T>
auto id(const T& txt) -> auto
{
    return [&] { return txt; };
}

void SelectMenu(size_t menuIdx);

LicenseMenu::LicenseMenu()
    : Menu("openRBRVR licenses", {})
    , menuScroll(0)
{
    for (const auto& row : gLicenseInformation) {
        this->entries.push_back({ .text = id(row), .font = IRBRGame::EFonts::FONT_SMALL, .menuColor = IRBRGame::EMenuColors::MENU_TEXT });
    }
}
void LicenseMenu::Left()
{
    SelectMenu(0);
}
void LicenseMenu::Select()
{
    SelectMenu(0);
}

void RecenterVR()
{
    auto chaperone = vr::VRChaperone();
    if (chaperone) {
        chaperone->ResetZeroPose(vr::ETrackingUniverseOrigin::TrackingUniverseSeated);
    }
}

static std::string GetHorizonLockStr()
{
    switch (gCfg.lockToHorizon) {
        case Config::HorizonLock::LOCK_NONE:
            return "Off";
        case Config::HorizonLock::LOCK_ROLL:
            return "Roll";
        case Config::HorizonLock::LOCK_PITCH:
            return "Pitch";
        case (Config::HorizonLock::LOCK_ROLL | Config::HorizonLock::LOCK_PITCH):
            return "Pitch and roll";
        default:
            return "Unknown";
    }
}

static void ChangeHorizonLock(bool forward)
{
    switch (gCfg.lockToHorizon) {
        case Config::HorizonLock::LOCK_NONE:
            gCfg.lockToHorizon = forward ? Config::HorizonLock::LOCK_ROLL : static_cast<Config::HorizonLock>((Config::HorizonLock::LOCK_ROLL | Config::HorizonLock::LOCK_PITCH));
            break;
        case Config::HorizonLock::LOCK_ROLL:
            gCfg.lockToHorizon = forward ? Config::HorizonLock::LOCK_PITCH : Config::HorizonLock::LOCK_NONE;
            break;
        case Config::HorizonLock::LOCK_PITCH:
            gCfg.lockToHorizon = forward ? static_cast<Config::HorizonLock>((Config::HorizonLock::LOCK_ROLL | Config::HorizonLock::LOCK_PITCH)) : Config::HorizonLock::LOCK_ROLL;
            break;
        case (Config::HorizonLock::LOCK_ROLL | Config::HorizonLock::LOCK_PITCH):
            gCfg.lockToHorizon = forward ? Config::HorizonLock::LOCK_NONE : Config::HorizonLock::LOCK_PITCH;
            break;
        default:
            gCfg.lockToHorizon = Config::HorizonLock::LOCK_NONE;
            break;
    }
}

void ChangeRenderIn3dSettings(bool forward)
{
    if (forward && gCfg.renderMainMenu3d) {
        gCfg.renderMainMenu3d = false;
        gCfg.renderPreStage3d = false;
        gCfg.renderPauseMenu3d = false;
    } else if (forward) {
        gCfg.renderMainMenu3d = gCfg.renderPreStage3d;
        gCfg.renderPreStage3d = gCfg.renderPauseMenu3d;
        gCfg.renderPauseMenu3d = true;
    } else {
        gCfg.renderPauseMenu3d = gCfg.renderPreStage3d;
        gCfg.renderPreStage3d = gCfg.renderMainMenu3d;
        gCfg.renderMainMenu3d = false;
    }
}

void Toggle(bool& value) { value = !value; }

// clang-format off
static Menu mainMenu = { "openRBRVR", {
  { id("Recenter"), "Recenters VR view", std::nullopt, IRBRGame::EMenuColors::MENU_TEXT, std::nullopt, Menu::menuItemsStartPos, std::nullopt, std::nullopt, RecenterVR },
  { .text = [] { return std::format("Lock horizon: {}", GetHorizonLockStr()); },
    .leftAction = [] { ChangeHorizonLock(false); },
    .rightAction = [] { ChangeHorizonLock(true); },
    .selectAction = [] { ChangeHorizonLock(true); },
  },
  { .text = id("Graphics settings") , .longText = "Graphics settings", .selectAction = [] { SelectMenu(1); } },
  { .text = id("Debug settings"), .longText = "Debug settings. Not intended to be changed unless there is a problem that needs more information.", .selectAction = [] { SelectMenu(2); } },
  { .text = id("Licenses"), .longText = "License information of open source libraries used in the plugin's implementation.", .selectAction = [] { SelectMenu(3); } },
  { .text = id("Save the current config to openRBRVR.ini"),
    .color = [] { return (gCfg == gSavedCfg) ? std::make_tuple(0.5f, 0.5f, 0.5f, 1.0f) : std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f); },
    .selectAction = [] {
		if (gCfg.Write("Plugins\\openRBRVR.ini")) {
			gSavedCfg = gCfg;
		}
    }
  },
}};
static Menu graphicsMenu = { "openRBRVR graphics settings", {
  { .text = [] { return std::format("Draw desktop window: {}", gCfg.drawCompanionWindow ? "ON" : "OFF"); },
    .menuColor = IRBRGame::EMenuColors::MENU_TEXT,
    .position = Menu::menuItemsStartPos,
    .leftAction = [] { Toggle(gCfg.drawCompanionWindow); },
    .rightAction = [] { Toggle(gCfg.drawCompanionWindow); },
    .selectAction = [] { Toggle(gCfg.drawCompanionWindow); },
  },
  { .text = [] { return std::format("Draw loading screen: {}", gCfg.drawLoadingScreen ? "ON" : "OFF"); },
    .leftAction = [] { Toggle(gCfg.drawLoadingScreen); },
    .rightAction = [] { Toggle(gCfg.drawLoadingScreen); },
    .selectAction = [] { Toggle(gCfg.drawLoadingScreen); },
  },
  { .text = [] { return std::format("Render in 3D: [{}] pause menu, [{}] pre-stage, [{}] main menu",
      gCfg.renderPauseMenu3d ? "x" : " ",
      gCfg.renderPreStage3d ? "x" : " ",
      gCfg.renderMainMenu3d ? "x" : " ");
    },
    .leftAction = [] { ChangeRenderIn3dSettings(false); },
    .rightAction = [] { ChangeRenderIn3dSettings(true); },
    .selectAction = [] { ChangeRenderIn3dSettings(true); },
  },
  { .text = id("Back to previous menu"),
	.selectAction = [] { SelectMenu(0); }
  },
}};
static Menu debugMenu = { "openRBRVR debug settings", {
  { .text = [] { return std::format("Debug information: {}", gCfg.debug ? "ON" : "OFF"); },
    .menuColor = IRBRGame::EMenuColors::MENU_TEXT,
	.position = Menu::menuItemsStartPos,
    .leftAction = [] { Toggle(gCfg.debug); },
    .rightAction = [] { Toggle(gCfg.debug); },
    .selectAction = [] { Toggle(gCfg.debug); },
  },
  { .text = id("Back to previous menu"),
	.selectAction = [] { SelectMenu(0); }
  },
}};
static LicenseMenu licenseMenu;
// clang-format on

static auto menus = std::to_array<Menu*>({
    &mainMenu,
    &graphicsMenu,
    &debugMenu,
    &licenseMenu,
});

Menu* gMenu = menus[0];

void SelectMenu(size_t menuIdx)
{
    gMenu = menus[menuIdx % menus.size()];
}
