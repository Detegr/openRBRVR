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

static void ChangeRenderIn3dSettings(bool forward)
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
static class Menu mainMenu = { "openRBRVR", {
  { .text = id("Recenter VR view"), .longText = {"Recenters VR view"}, .menuColor = IRBRGame::EMenuColors::MENU_TEXT, .position = Menu::menuItemsStartPos, .selectAction = RecenterVR },
  { .text = [] { return std::format("Lock horizon: {}", GetHorizonLockStr()); },
    .longText = {
        "Enable to rotate the car around the headset instead of rotating the headset with the car.",
        "For some people, enabling this option gives a more comfortable VR experience.",
        "Roll means locking the left-right axis.",
        "Pitch means locking the front-back axis."
    },
    .leftAction = [] { ChangeHorizonLock(false); },
    .rightAction = [] { ChangeHorizonLock(true); },
    .selectAction = [] { ChangeHorizonLock(true); },
  },
  { .text = id("Graphics settings") , .longText = {"Graphics settings"}, .selectAction = [] { SelectMenu(1); } },
  { .text = id("Debug settings"), .longText = {"Not intended to be changed unless there is a problem that needs more information."}, .selectAction = [] { SelectMenu(2); } },
  { .text = [] { return std::format("VR runtime: {}", gCfg.runtime == OPENVR ? "OpenVR (SteamVR)" : "OpenXR"); },
    .longText {"Selects VR runtime. Requires game restart.", "", "SteamVR support is more mature and supports more devices.", "", "OpenXR is an open-source, royalty-free standard.", "It has less overhead and may result in better performance.", "", "OpenXR device compatibility is more limited for old 32-bit games like RBR."},
    .leftAction = [] { gCfg.runtime = gCfg.runtime == OPENXR ? OPENVR : OPENXR; },
    .rightAction = [] { gCfg.runtime = gCfg.runtime == OPENXR ? OPENVR : OPENXR; },
    .selectAction = [] { gCfg.runtime = gCfg.runtime == OPENXR ? OPENVR : OPENXR; },
  },
  { .text = id("Licenses"), .longText = {"License information of open source libraries used in the plugin's implementation."}, .selectAction = [] { SelectMenu(3); } },
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
    .longText = { "Draw game window on the monitor.", "Found to have a performance impact on some machines."},
    .menuColor = IRBRGame::EMenuColors::MENU_TEXT,
    .position = Menu::menuItemsStartPos,
    .leftAction = [] { Toggle(gCfg.drawCompanionWindow); },
    .rightAction = [] { Toggle(gCfg.drawCompanionWindow); },
    .selectAction = [] { Toggle(gCfg.drawCompanionWindow); },
  },
  { .text = [] { return std::format("Draw loading screen: {}", gCfg.drawLoadingScreen ? "ON" : "OFF"); },
    .longText = { "If the screen flickers while loading, turn this OFF to have a black screen while loading."},
    .leftAction = [] { Toggle(gCfg.drawLoadingScreen); },
    .rightAction = [] { Toggle(gCfg.drawLoadingScreen); },
    .selectAction = [] { Toggle(gCfg.drawLoadingScreen); },
  },
  { .text = [] { return std::format("Render pause menu in 3D: {}", gCfg.renderPauseMenu3d ? "ON" : "OFF"); },
    .longText = { "Enable to render the pause menu of the game in 3D overlay instead of a 2D plane." },
    .leftAction = [] { Toggle(gCfg.renderPauseMenu3d); },
    .rightAction = [] { Toggle(gCfg.renderPauseMenu3d); },
    .selectAction = [] { Toggle(gCfg.renderPauseMenu3d); },
  },
  { .text = [] { return std::format("Render prestage animation in 3D: {}", gCfg.renderPreStage3d ? "ON" : "OFF"); },
    .longText = { "Enable to render the spinning animation around the car of the game in 3D instead of a 2D plane.", "Enabling this option may cause discomfort to some people."},
    .leftAction = [] { Toggle(gCfg.renderPreStage3d); },
    .rightAction = [] { Toggle(gCfg.renderPreStage3d); },
    .selectAction = [] { Toggle(gCfg.renderPreStage3d); },
  },
  { .text = [] { return std::format("Render main menu in 3D: {}", gCfg.renderMainMenu3d ? "ON" : "OFF"); },
    .longText = { "Enable to render the main menu in 3D instead of 2D plane.", "Enabling this option may cause discomfort to some people."},
    .leftAction = [] { Toggle(gCfg.renderMainMenu3d); },
    .rightAction = [] { Toggle(gCfg.renderMainMenu3d); },
    .selectAction = [] { Toggle(gCfg.renderMainMenu3d); },
  },
  { .text = [] { return std::format("Render replays in 3D: {}", gCfg.renderReplays3d ? "ON" : "OFF"); },
    .longText = { "Enable to render replays in 3D instead of a 2D plane." },
    .leftAction = [] { Toggle(gCfg.renderReplays3d); },
    .rightAction = [] { Toggle(gCfg.renderReplays3d); },
    .selectAction = [] { Toggle(gCfg.renderReplays3d); },
  },
  { .text = id("Back to previous menu"),
	.selectAction = [] { SelectMenu(0); }
  },
}};
static Menu debugMenu = { "openRBRVR debug settings", {
  { .text = [] { return std::format("Debug information: {}", gCfg.debug ? "ON" : "OFF"); },
    .longText = { "Show a lot of technical information on the top-left of the screen." },
    .menuColor = IRBRGame::EMenuColors::MENU_TEXT,
	.position = Menu::menuItemsStartPos,
    .leftAction = [] { Toggle(gCfg.debug); },
    .rightAction = [] { Toggle(gCfg.debug); },
    .selectAction = [] { Toggle(gCfg.debug); },
  },
  { .text = [] { return std::format("OpenXR Reverb compatibility mode: {}", gCfg.wmrWorkaround ? "ON" : "OFF"); },
    .longText = { "A workaround making OpenXR mode work with WMR OpenXR runtime.", "This has an performance impact, should be set to off if OpenXR runtime works without this option.", "Has no effect in SteamVR." },
    .menuColor = IRBRGame::EMenuColors::MENU_TEXT,
    .leftAction = [] { Toggle(gCfg.wmrWorkaround); },
    .rightAction = [] { Toggle(gCfg.wmrWorkaround); },
    .selectAction = [] { Toggle(gCfg.wmrWorkaround); },
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
