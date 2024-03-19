#include <d3d9.h>
#include <format>
#include <gtx/matrix_decompose.hpp>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "Config.hpp"
#include "D3D.hpp"
#include "Dx.hpp"
#include "Globals.hpp"
#include "Hook.hpp"
#include "Licenses.hpp"
#include "Menu.hpp"
#include "OpenVR.hpp"
#include "OpenXR.hpp"
#include "Quaternion.hpp"
#include "RBR.hpp"
#include "Util.hpp"
#include "VR.hpp"
#include "Version.hpp"
#include "Vertex.hpp"
#include "openRBRVR.hpp"

openRBRVR::openRBRVR(IRBRGame* g)
    : game(g)
{
    g::game = g;
    dbg("Hooking DirectX");

    auto d3ddll = GetModuleHandle("d3d9.dll");
    if (!d3ddll) {
        dbg("failed to get handle to d3d9.dll\n");
        return;
    }
    auto d3dcreate = reinterpret_cast<decltype(&dx::Direct3DCreate9)>(GetProcAddress(d3ddll, "Direct3DCreate9"));
    if (!d3dcreate) {
        dbg("failed to find address to Direct3DCreate9\n");
        return;
    }

    try {
        g::hooks::create = Hook(d3dcreate, dx::Direct3DCreate9);
        g::hooks::render = Hook(*reinterpret_cast<decltype(rbr::render)*>(rbr::get_render_function_addr()), rbr::render);
        g::hooks::render_particles = Hook(*reinterpret_cast<decltype(rbr::render_particles)*>(rbr::get_render_particles_function_addr()), rbr::render_particles);
    } catch (const std::runtime_error& e) {
        dbg(e.what());
        MessageBoxA(nullptr, e.what(), "Hooking failed", MB_OK);
    }
    g::cfg = g::saved_cfg = Config::fromPath("Plugins");
    g::draw_overlay_border = (g::cfg.debug && g::cfg.debug_mode == 0);
}

openRBRVR::~openRBRVR()
{
    if (g::vr) {
        g::vr->shutdown_vr();
        delete g::vr;
    }
}

void openRBRVR::HandleFrontEndEvents(char txtKeyboard, bool bUp, bool bDown, bool bLeft, bool bRight, bool bSelect)
{
    if (bSelect) {
        g::menu->select();
    }
    if (bUp) {
        g::menu->up();
    }
    if (bDown) {
        g::menu->down();
    }
    if (bLeft) {
        g::menu->left();
    }
    if (bRight) {
        g::menu->right();
    }
}

void openRBRVR::DrawMenuEntries(const std::ranges::forward_range auto& entries, float rowHeight)
{
    auto x = 0.0f;
    auto y = 0.0f;
    for (const auto& entry : entries) {
        if (entry.visible && !entry.visible.value()()) {
            // Skip hidden entries
            continue;
        }
        if (entry.font) {
            game->SetFont(entry.font.value());
        }
        if (entry.menu_color) {
            game->SetMenuColor(entry.menu_color.value());
        } else if (entry.color) {
            auto [r, g, b, a] = entry.color.value()();
            game->SetColor(r, g, b, a);
        }
        if (entry.position) {
            auto [newx, newy] = entry.position.value();
            x = newx;
            y = newy;
        }
        game->WriteText(x, y, entry.text().c_str());
        y += rowHeight;
    }
    game->SetFont(IRBRGame::EFonts::FONT_SMALL);
    game->SetColor(0.7f, 0.7f, 0.7f, 1.0f);
    game->WriteText(10.0f, 458.0f, std::format("openRBRVR {} - https://github.com/Detegr/openRBRVR", VERSION_STR).c_str());
}

void openRBRVR::DrawFrontEndPage()
{
    constexpr auto menuItemsStartHeight = 70.0f;
    const auto idx = g::menu->index();
    const auto isLicenseMenu = idx < 0;
    const auto visibleEntryCount = std::ranges::count_if(g::menu->entries(), [](const MenuEntry& e) {
        return !e.visible || e.visible.value()();
    });
    const auto endOfItems = menuItemsStartHeight + visibleEntryCount * g::menu->row_height();

    game->DrawBlackOut(0.0f, isLicenseMenu ? 88.0f : endOfItems, 800.0f, 10.0f);
    if (!isLicenseMenu) {
        game->DrawSelection(0.0f, menuItemsStartHeight - 2.0f + (static_cast<float>(g::menu->index()) * g::menu->row_height()), 440.0f);
    }

    game->SetFont(IRBRGame::EFonts::FONT_BIG);
    game->SetMenuColor(IRBRGame::EMenuColors::MENU_HEADING);
    game->WriteText(65.0f, 49.0f, g::menu->heading());

    const auto& entries = g::menu->entries();
    DrawMenuEntries(entries, g::menu->row_height());

    game->SetFont(IRBRGame::EFonts::FONT_BIG);
    game->SetMenuColor(IRBRGame::EMenuColors::MENU_TEXT);

    auto i = 0;
    if (!isLicenseMenu) {
        for (const auto& txt : entries[g::menu->entry_idx].long_text) {
            game->WriteText(65.0f, endOfItems + ((i + 1) * g::menu->row_height()), txt.c_str());
            i++;
        }
    }
}
