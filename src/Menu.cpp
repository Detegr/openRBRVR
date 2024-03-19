#include "Menu.hpp"
#include "Config.hpp"
#include "Globals.hpp"
#include "VR.hpp"

#include <openvr.h>

#include <array>
#include <format>

void select_menu(size_t menuIdx);

// Helper function to create an identity function to return its argument
template <typename T>
auto id(const T& txt) -> auto
{
    return [&] { return txt; };
}

LicenseMenu::LicenseMenu()
    : Menu("openRBRVR licenses", {})
    , menuScroll(0)
{
    for (const auto& row : g::license_information) {
        this->menu_entries.push_back({ .text = id(row), .font = IRBRGame::EFonts::FONT_SMALL, .menu_color = IRBRGame::EMenuColors::MENU_TEXT });
    }
}
void LicenseMenu::left()
{
    select_menu(0);
}
void LicenseMenu::select()
{
    select_menu(0);
}

void recenter_vr()
{
    auto chaperone = vr::VRChaperone();
    if (chaperone) {
        chaperone->ResetZeroPose(vr::ETrackingUniverseOrigin::TrackingUniverseSeated);
    }
}

static std::string get_horizon_lock_str()
{
    switch (g::cfg.lock_to_horizon) {
        case HorizonLock::LOCK_NONE:
            return "Off";
        case HorizonLock::LOCK_ROLL:
            return "Roll";
        case HorizonLock::LOCK_PITCH:
            return "Pitch";
        case (HorizonLock::LOCK_ROLL | HorizonLock::LOCK_PITCH):
            return "Pitch and roll";
        default:
            return "Unknown";
    }
}

static void change_horizon_lock(bool forward)
{
    switch (g::cfg.lock_to_horizon) {
        case HorizonLock::LOCK_NONE:
            g::cfg.lock_to_horizon = forward ? HorizonLock::LOCK_ROLL : static_cast<HorizonLock>((HorizonLock::LOCK_ROLL | HorizonLock::LOCK_PITCH));
            break;
        case HorizonLock::LOCK_ROLL:
            g::cfg.lock_to_horizon = forward ? HorizonLock::LOCK_PITCH : HorizonLock::LOCK_NONE;
            break;
        case HorizonLock::LOCK_PITCH:
            g::cfg.lock_to_horizon = forward ? static_cast<HorizonLock>((HorizonLock::LOCK_ROLL | HorizonLock::LOCK_PITCH)) : HorizonLock::LOCK_ROLL;
            break;
        case (HorizonLock::LOCK_ROLL | HorizonLock::LOCK_PITCH):
            g::cfg.lock_to_horizon = forward ? HorizonLock::LOCK_NONE : HorizonLock::LOCK_PITCH;
            break;
        default:
            g::cfg.lock_to_horizon = HorizonLock::LOCK_NONE;
            break;
    }
}

static void change_render_3d_settings(bool forward)
{
    if (forward && g::cfg.render_mainmenu_3d) {
        g::cfg.render_mainmenu_3d = false;
        g::cfg.render_prestage_3d = false;
        g::cfg.render_pausemenu_3d = false;
    } else if (forward) {
        g::cfg.render_mainmenu_3d = g::cfg.render_prestage_3d;
        g::cfg.render_prestage_3d = g::cfg.render_pausemenu_3d;
        g::cfg.render_pausemenu_3d = true;
    } else {
        g::cfg.render_pausemenu_3d = g::cfg.render_prestage_3d;
        g::cfg.render_prestage_3d = g::cfg.render_mainmenu_3d;
        g::cfg.render_mainmenu_3d = false;
    }
}

static void ChangeCompanionMode(bool forward)
{
    if (forward) {
        if (g::cfg.companion_mode == CompanionMode::Off) {
            g::cfg.companion_mode = CompanionMode::VREye;
        } else if (g::cfg.companion_mode == CompanionMode::VREye) {
            g::cfg.companion_mode = CompanionMode::Static;
        } else {
            g::cfg.companion_mode = CompanionMode::Off;
        }
    } else {
        if (g::cfg.companion_mode == CompanionMode::Off) {
            g::cfg.companion_mode = CompanionMode::Static;
        } else if (g::cfg.companion_mode == CompanionMode::Static) {
            g::cfg.companion_mode = CompanionMode::VREye;
        } else {
            g::cfg.companion_mode = CompanionMode::Off;
        }
    }
}

void Toggle(bool& value) { value = !value; }
void Toggle(int& value)
{
    if (value == 0) {
        value = 1;
    } else {
        value = 0;
    }
}

// clang-format off
static class Menu main_menu = { "openRBRVR", {
  { .text = id("Recenter VR view"), .long_text = {"Recenters VR view"}, .menu_color = IRBRGame::EMenuColors::MENU_TEXT, .position = Menu::menu_items_start_pos, .select_action = recenter_vr },
  { .text = id("Horizon lock settings") , .long_text = {"Horizon lock settings"}, .select_action = [] { select_menu(4); } },
  { .text = id("Rendering settings") , .long_text = {"Selection of different rendering settings"}, .select_action = [] { select_menu(1); } },
  { .text = id("Overlay settings") , .long_text = {"Adjust the size and position of the 2D content shown on", "top of the 3D view while driving."}, .select_action = [] { select_menu(5); } },
  { .text = id("Desktop window settings") ,
    .long_text = {"Adjust the size and position of the image shown", "on the desktop window while driving."},
    .select_action = [] { if(g::vr) select_menu(6); },
    .visible = [] { return g::vr != nullptr; }
  },
  { .text = id("Debug settings"), .long_text = {"Not intended to be changed unless there is a problem that needs more information."}, .select_action = [] { select_menu(2); } },
  { .text = [] { return std::format("VR runtime: {}", g::cfg.runtime == OPENVR ? "OpenVR (SteamVR)" : (g::cfg.wmr_workaround ? "OpenXR (Reverb compatibility mode)" : "OpenXR")); },
    .long_text {
        "Selects VR runtime. Requires game restart.",
        "",
        "SteamVR support is more mature and supports more devices.",
        "OpenXR is an open-source, royalty-free standard.",
        "It has less overhead and may result in better performance.",
        "OpenXR device compatibility is more limited for old 32-bit games like RBR.",
        "The performance of Reverb compatibility mode is worse than normal OpenXR.",
        "Use it only if there's problems with the normal mode."},
    .left_action = [] {
        if (g::cfg.runtime == OPENXR) { if(g::cfg.wmr_workaround) Toggle(g::cfg.wmr_workaround); else g::cfg.runtime = OPENVR; }
        else { g::cfg.runtime = OPENXR; g::cfg.wmr_workaround = true; }
    },
    .right_action = [] {
        if (g::cfg.runtime == OPENXR) { if(g::cfg.wmr_workaround) g::cfg.runtime = OPENVR; else g::cfg.wmr_workaround = true; }
        else { g::cfg.runtime = OPENXR; g::cfg.wmr_workaround = false; }
    },
    .select_action = [] {},
  },
  { .text = id("OpenXR settings"), .long_text = {"OpenXR settings"}, .select_action = [] { select_menu(7); }, .visible = [] { return g::cfg.runtime == VRRuntime::OPENXR; }},
  { .text = id("Licenses"), .long_text = {"License information of open source libraries used in the plugin's implementation."}, .select_action = [] { select_menu(3); } },
  { .text = id("Save the current config to openRBRVR.toml"),
    .color = [] { return (g::cfg == g::saved_cfg) ? std::make_tuple(0.5f, 0.5f, 0.5f, 1.0f) : std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f); },
    .select_action = [] {
        if (g::cfg.Write("Plugins\\openRBRVR.toml")) {
            g::saved_cfg = g::cfg;
        }
    }
  },
}};

static class Menu graphics_menu = { "openRBRVR rendering settings", {
  { .text = [] { return std::format("Render loading screen: {}", g::cfg.draw_loading_screen ? "ON" : "OFF"); },
    .long_text = { "If the screen flickers while loading, turn this OFF to have a black screen while loading."},
    .menu_color = IRBRGame::EMenuColors::MENU_TEXT,
    .position = Menu::menu_items_start_pos,
    .left_action = [] { Toggle(g::cfg.draw_loading_screen); },
    .right_action = [] { Toggle(g::cfg.draw_loading_screen); },
    .select_action = [] { Toggle(g::cfg.draw_loading_screen); },
  },
  { .text = [] { return std::format("Render pause menu in 3D: {}", g::cfg.render_pausemenu_3d ? "ON" : "OFF"); },
    .long_text = { "Enable to render the pause menu of the game in 3D overlay instead of a 2D plane." },
    .left_action = [] { Toggle(g::cfg.render_pausemenu_3d); },
    .right_action = [] { Toggle(g::cfg.render_pausemenu_3d); },
    .select_action = [] { Toggle(g::cfg.render_pausemenu_3d); },
  },
  { .text = [] { return std::format("Render prestage animation in 3D: {}", g::cfg.render_prestage_3d ? "ON" : "OFF"); },
    .long_text = { "Enable to render the spinning animation around the car of the game in 3D instead of a 2D plane.", "Enabling this option may cause discomfort to some people."},
    .left_action = [] { Toggle(g::cfg.render_prestage_3d); },
    .right_action = [] { Toggle(g::cfg.render_prestage_3d); },
    .select_action = [] { Toggle(g::cfg.render_prestage_3d); },
  },
  { .text = [] { return std::format("Render main menu in 3D: {}", g::cfg.render_mainmenu_3d ? "ON" : "OFF"); },
    .long_text = { "Enable to render the main menu in 3D instead of 2D plane.", "Enabling this option may cause discomfort to some people."},
    .left_action = [] { Toggle(g::cfg.render_mainmenu_3d); },
    .right_action = [] { Toggle(g::cfg.render_mainmenu_3d); },
    .select_action = [] { Toggle(g::cfg.render_mainmenu_3d); },
  },
  { .text = [] { return std::format("Render replays in 3D: {}", g::cfg.render_replays_3d ? "ON" : "OFF"); },
    .long_text = { "Enable to render replays in 3D instead of a 2D plane." },
    .left_action = [] { Toggle(g::cfg.render_replays_3d); },
    .right_action = [] { Toggle(g::cfg.render_replays_3d); },
    .select_action = [] { Toggle(g::cfg.render_replays_3d); },
  },
  { .text = id("Back to previous menu"),
	.select_action = [] { select_menu(0); }
  },
}};

static class Menu debug_menu = { "openRBRVR debug settings", {
  { .text = [] { return std::format("Debug information: {}", g::cfg.debug ? "ON" : "OFF"); },
    .long_text = { "Show a debug information on top-left of the screen.", "Choose below if you want to see everything or just a FPS counter." },
    .menu_color = IRBRGame::EMenuColors::MENU_TEXT,
	.position = Menu::menu_items_start_pos,
    .left_action = [] { Toggle(g::cfg.debug); g::draw_overlay_border = (g::cfg.debug && g::cfg.debug_mode == 0); },
    .right_action = [] { Toggle(g::cfg.debug); g::draw_overlay_border = (g::cfg.debug && g::cfg.debug_mode == 0); },
    .select_action = [] { Toggle(g::cfg.debug); g::draw_overlay_border = (g::cfg.debug && g::cfg.debug_mode == 0); },
  },
  { .text = [] { return std::format("Debug info contents: {}", g::cfg.debug_mode == 0 ? "All" : "FPS only"); },
    .long_text = { "All: Show everything", "FPS only: show colored FPS counter with following logic:", "- Less than 70 percents of available frame time used: GREEN", "- More than 70 percents of available frame time used: YELLOW", "- Frame time exceeds available frame time: RED"},
    .left_action = [] { Toggle(g::cfg.debug_mode); g::draw_overlay_border = (g::cfg.debug && g::cfg.debug_mode == 0); },
    .right_action = [] { Toggle(g::cfg.debug_mode); g::draw_overlay_border = (g::cfg.debug && g::cfg.debug_mode == 0); },
  },
  { .text = id("Back to previous menu"),
	.select_action = [] { select_menu(0); }
  },
}};

static LicenseMenu license_menu;

static class Menu horizon_lock_menu = { "openRBRVR horizon lock settings", {
  { .text = [] { return std::format("Lock horizon: {}", get_horizon_lock_str()); },
    .long_text = {
        "Enable to rotate the car around the headset instead of rotating the headset with the car.",
        "For some people, enabling this option gives a more comfortable VR experience.",
        "Roll means locking the left-right axis.",
        "Pitch means locking the front-back axis."
    },
    .menu_color = IRBRGame::EMenuColors::MENU_TEXT,
    .position = Menu::menu_items_start_pos,
    .left_action = [] { change_horizon_lock(false); },
    .right_action = [] { change_horizon_lock(true); },
    .select_action = [] { change_horizon_lock(true); },
  },
  { .text = [] { return std::format("Percentage: {}%", static_cast<int>(g::cfg.horizon_lock_multiplier * 100.0)); },
    .long_text = {
        "Amount of locking that's happening. 100 means the horizon is always level.",
    },
    .left_action = [] { g::cfg.horizon_lock_multiplier = std::max<double>(0.05, (g::cfg.horizon_lock_multiplier * 100.0 - 5) / 100.0); },
    .right_action = [] { g::cfg.horizon_lock_multiplier = std::min<double>(1.0, (g::cfg.horizon_lock_multiplier * 100.0 + 5) / 100.0); },
  },
  { .text = id("Back to previous menu"),
    .select_action = [] { select_menu(0); }
  },
}};

static class Menu overlay_menu = { "openRBRVR overlay settings", {
  { .text = [] { return std::format("Menu size: {:.2f}", g::cfg.menu_size); },
    .long_text = { "Adjust menu size." },
    .menu_color = IRBRGame::EMenuColors::MENU_TEXT,
	.position = Menu::menu_items_start_pos,
    .left_action = [] { g::cfg.menu_size = std::max<float>((g::cfg.menu_size - 0.05f), 0.05f); },
    .right_action = [] { g::cfg.menu_size = std::min<float>((g::cfg.menu_size + 0.05f), 10.0f); },
  },
  { .text = [] { return std::format("Overlay size: {:.2f}", g::cfg.overlay_size); },
    .long_text = { "Adjust overlay size. Overlay is the area where 2D content", "is rendered when driving."},
    .left_action = [] { g::cfg.overlay_size = std::max<float>((g::cfg.overlay_size - 0.05f), 0.05f); },
    .right_action = [] { g::cfg.overlay_size = std::min<float>((g::cfg.overlay_size + 0.05f), 10.0f); },
  },
  { .text = [] { return std::format("Overlay X position: {:.2f}", g::cfg.overlay_translation.x); },
    .long_text = { "Adjust overlay position. Overlay is the area where 2D content", "is rendered when driving."},
    .left_action = [] { g::cfg.overlay_translation.x -= 0.01f; },
    .right_action = [] { g::cfg.overlay_translation.x += 0.01f; },
  },
  { .text = [] { return std::format("Overlay Y position: {:.2f}", g::cfg.overlay_translation.y); },
    .long_text = { "Adjust overlay position. Overlay is the area where 2D content", "is rendered when driving."},
    .left_action = [] { g::cfg.overlay_translation.y -= 0.01f; },
    .right_action = [] { g::cfg.overlay_translation.y += 0.01f; },
  },
  { .text = [] { return std::format("Overlay Z position: {:.2f}", g::cfg.overlay_translation.z); },
    .long_text = { "Adjust overlay position. Overlay is the area where 2D content", "is rendered when driving."},
    .left_action = [] { g::cfg.overlay_translation.z -= 0.01f; },
    .right_action = [] { g::cfg.overlay_translation.z += 0.01f; },
  },
  { .text = id("Back to previous menu"),
    .select_action = [] { select_menu(0); },
  },
}};

static const auto window_step = 1;
static class Menu companion_menu = { "openRBRVR desktop window settings", {
  { .text = [] { return std::format("Desktop window mode: {}", companion_mode_str_pretty(g::cfg.companion_mode)); },
    .long_text = { "Choose what is visible on the desktop monitor while driving.", "Off: Don't draw anything", "VR view: Draw what is seen in the VR headset", "Bonnet camera: Use normal 2D bonnet camera (WITH SIGNIFICANT PERFORMANCE COST)"},
    .menu_color = IRBRGame::EMenuColors::MENU_TEXT,
    .position = Menu::menu_items_start_pos,
    .left_action = [] { ChangeCompanionMode(false); },
    .right_action = [] { ChangeCompanionMode(true); },
  },
  {.text = [] { return std::format("Desktop window rendering area size: {} ({:.0f}x{:.0f} pixels)", g::cfg.companion_size, std::round(std::get<0>(g::vr->get_render_resolution(LeftEye)) * (g::cfg.companion_size / 100.0)), std::round(std::get<1>(g::vr->get_render_resolution(LeftEye)) * (g::cfg.companion_size / 100.0 / g::vr->aspect_ratio))); },
    .long_text = { "Adjust rendering area size. The value is percentage of the width.", "For example, setting this to 50 will render half width of the full resolution", "resolution VR texture." },
    .color = [] { return (g::cfg.companion_mode == CompanionMode::VREye) ? std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f) : std::make_tuple(0.5f, 0.5f, 0.5f, 1.0f); },
    .left_action = [] {
        g::cfg.companion_size = std::clamp(g::cfg.companion_size - window_step, 10, 100);
        g::cfg.companion_offset.x = std::min(g::cfg.companion_offset.x, 100 - g::cfg.companion_size);
        g::cfg.companion_offset.y = std::min(g::cfg.companion_offset.y, 100 - static_cast<int>(std::round(g::cfg.companion_size / g::vr->aspect_ratio)));
        g::vr->create_companion_window_buffer(g::d3d_dev);
    },
    .right_action = [] {
        g::cfg.companion_size = std::clamp(g::cfg.companion_size + window_step, 10, 100);
        g::cfg.companion_offset.x = std::min(g::cfg.companion_offset.x, 100 - g::cfg.companion_size);
        g::cfg.companion_offset.y = std::min(g::cfg.companion_offset.y, 100 - static_cast<int>(std::round(g::cfg.companion_size / g::vr->aspect_ratio)));
        g::vr->create_companion_window_buffer(g::d3d_dev);
    },
  },
  {.text = [] { return std::format("X offset: {} ({:.0f} pixels)", g::cfg.companion_offset.x, std::floor(std::get<0>(g::vr->get_render_resolution(LeftEye)) * g::cfg.companion_offset.x / 100.0f)); },
    .long_text = { "X offset in percents from the left side of the screen." },
    .color = [] { return (g::cfg.companion_mode == CompanionMode::VREye) ? std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f) : std::make_tuple(0.5f, 0.5f, 0.5f, 1.0f); },
    .left_action = [] {
        g::cfg.companion_offset.x = std::clamp(g::cfg.companion_offset.x - window_step, 0, 100 - g::cfg.companion_size);
        g::vr->create_companion_window_buffer(g::d3d_dev);
    },
    .right_action = [] {
        g::cfg.companion_offset.x = std::clamp(g::cfg.companion_offset.x + window_step, 0, 100 - g::cfg.companion_size);
        g::vr->create_companion_window_buffer(g::d3d_dev);
    },
  },
  { .text = [] { return std::format("Y offset: {} ({:.0f} pixels)", g::cfg.companion_offset.y, std::floor(std::get<1>(g::vr->get_render_resolution(LeftEye)) * g::cfg.companion_offset.y / 100.0f)); },
    .long_text = { "Y offset in percents from the top of the screen." },
    .color = [] { return (g::cfg.companion_mode == CompanionMode::VREye) ? std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f) : std::make_tuple(0.5f, 0.5f, 0.5f, 1.0f); },
    .left_action = [] {
        g::cfg.companion_offset.y = std::clamp(g::cfg.companion_offset.y - window_step, 0, 100 - static_cast<int>(std::round(g::cfg.companion_size / g::vr->aspect_ratio)));
		g::vr->create_companion_window_buffer(g::d3d_dev);
    },
    .right_action = [] {
        g::cfg.companion_offset.y = std::clamp(g::cfg.companion_offset.y + window_step, 0, 100 - static_cast<int>(std::round(g::cfg.companion_size / g::vr->aspect_ratio)));
        g::vr->create_companion_window_buffer(g::d3d_dev);
    },
  },
  { .text = [] { return std::format("Eye: {}", g::cfg.companion_eye == LeftEye ? "Left eye" : "Right eye"); },
    .long_text = { "Eye that is being used to render the desktop window." },
    .color = [] { return (g::cfg.companion_mode == CompanionMode::VREye) ? std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f) : std::make_tuple(0.5f, 0.5f, 0.5f, 1.0f); },
    .left_action = [] { g::cfg.companion_eye = g::cfg.companion_eye == LeftEye ? RightEye : LeftEye; },
    .right_action = [] { g::cfg.companion_eye = g::cfg.companion_eye == LeftEye ? RightEye : LeftEye; },
  },
  { .text = id("Back to previous menu"),
    .menu_color = IRBRGame::EMenuColors::MENU_TEXT,
    .select_action = [] { select_menu(0); },
  },
}};

constexpr auto world_scale_step = 1;
static class Menu openxr_menu = { "openRBRVR OpenXR settings", {
  { .text = [] { return std::format("World scale: {:.01f}", g::cfg.world_scale / 10.0); },
    .long_text = { "Adjust VR world scaling"},
    .menu_color = IRBRGame::EMenuColors::MENU_TEXT,
    .position = Menu::menu_items_start_pos,
    .left_action = [] { g::cfg.world_scale = std::max(g::cfg.world_scale - world_scale_step, 500); },
    .right_action = [] { g::cfg.world_scale = std::min(g::cfg.world_scale + world_scale_step, 1500); },
  },
  { .text = id("Back to previous menu"),
    .menu_color = IRBRGame::EMenuColors::MENU_TEXT,
    .select_action = [] { select_menu(0); },
  },
}};
// clang-format on

static constexpr auto menus = std::to_array<class Menu*>({
    &main_menu,
    &graphics_menu,
    &debug_menu,
    &license_menu,
    &horizon_lock_menu,
    &overlay_menu,
    &companion_menu,
    &openxr_menu,
});

Menu* g::menu = menus[0];

void select_menu(size_t menuIdx)
{
    g::menu = menus[menuIdx % menus.size()];
}
