#include <MinHook.h>
#include <d3d9.h>
#include <format>
#include <optional>
#include <vector>

#include "Config.hpp"
#include "D3D.hpp"
#include "Hook.hpp"
#include "Licenses.hpp"
#include "Util.hpp"
#include "VR.hpp"
#include "openRBRVR.hpp"

static IDirect3DDevice9* gD3Ddev = nullptr;
static Config gCfg;

static glm::mat4 gFlipZMatrix = {
    { 1, 0, 0, 0 },
    { 0, 1, 0, 0 },
    { 0, 0, -1, 0 },
    { 0, 0, 0, 1 },
};

namespace shader {
    static M4 gCurrentProjectionMatrix;
    static M4 gCurrentProjectionMatrixInverse;
    static M4 gCurrentViewMatrix;
    static M4 gCurrentViewMatrixInverse;
}

namespace fixedfunction {
    D3DMATRIX gCurrentProjectionMatrix;
    D3DMATRIX gCurrentViewMatrix;
}

constexpr uintptr_t RBRTrackIdAddr = 0x1660804;
constexpr uintptr_t RBRRenderFunctionAddr = 0x47E1E0;
constexpr uintptr_t RBRRXTrackStatusOffset = 0x608d0;
void __fastcall RBRHook_Render(void* p);

namespace hooks {
    Hook<decltype(&Direct3DCreate9)> create;
    Hook<decltype(IDirect3D9Vtbl::CreateDevice)> createdevice;
    Hook<decltype(IDirect3DDevice9Vtbl::SetVertexShaderConstantF)> setvertexshaderconstantf;
    Hook<decltype(IDirect3DDevice9Vtbl::SetTransform)> settransform;
    Hook<decltype(IDirect3DDevice9Vtbl::Present)> present;
    Hook<decltype(&RBRHook_Render)> render;
    Hook<decltype(IDirect3DDevice9Vtbl::CreateVertexShader)> createvertexshader;
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    MH_Initialize();
    return TRUE;
}

static std::optional<RenderTarget> gVRRenderTarget;
static IDirect3DSurface9 *gOriginalScreenTgt, *gOriginalDepthStencil;
static bool gDriving;
static uint32_t gGameMode;
static std::vector<IDirect3DVertexShader9*> gOriginalShaders;
static volatile uint8_t* gBTBTrackStatus;

static bool IsLoadingBTBStage()
{
    return gBTBTrackStatus && *gBTBTrackStatus == 1 && gGameMode == 5;
}

HRESULT __stdcall DXHook_CreateVertexShader(IDirect3DDevice9* This, const DWORD* pFunction, IDirect3DVertexShader9** ppShader)
{
    static int i = 0;
    auto ret = hooks::createvertexshader.call(This, pFunction, ppShader);
    if (i < 40) {
        // These are the original shaders for RBR that need
        // to be patched with the VR projection.
        gOriginalShaders.push_back(*ppShader);
    }
    i++;
    return ret;
}

// Call the RBR render function with a texture as the render target
// Even though the render pipeline changes the render target while rendering,
// the original render target is respected and restored at the end of the pipeline.
void RenderVREye(void* p, RenderTarget eye, bool clear = true)
{
    gVRRenderTarget = eye;
    PrepareVRRendering(gD3Ddev, eye, clear);
    hooks::render.call(p);
    FinishVRRendering(gD3Ddev, eye);
    gVRRenderTarget = std::nullopt;
}

// Render `renderTarget2d` on a plane for both eyes
void RenderVROverlay(RenderTarget renderTarget2D, bool clear)
{
    // Somehow the shader parameters still have an effect on RenderMenuQuad
    // even though we disable shaders there. Therefore gVRRenderTarget needs
    // to be set correctly. I don't understand how this works to be honest :D

    gVRRenderTarget = LeftEye;
    PrepareVRRendering(gD3Ddev, LeftEye, clear);
    RenderMenuQuad(gD3Ddev, LeftEye, renderTarget2D);
    FinishVRRendering(gD3Ddev, LeftEye);

    gVRRenderTarget = RightEye;
    PrepareVRRendering(gD3Ddev, RightEye, clear);
    RenderMenuQuad(gD3Ddev, RightEye, renderTarget2D);
    FinishVRRendering(gD3Ddev, RightEye);

    gVRRenderTarget = std::nullopt;
}

// RBR 3D scene draw function is rerouted here
void __fastcall RBRHook_Render(void* p)
{
    auto ptr = reinterpret_cast<uintptr_t>(p);
    auto doRendering = *reinterpret_cast<uint32_t*>(ptr + 0x720) == 0;
    gGameMode = *reinterpret_cast<uint32_t*>(ptr + 0x728);
    gDriving = gGameMode == 1;

    gD3Ddev->GetRenderTarget(0, &gOriginalScreenTgt);
    gD3Ddev->GetDepthStencilSurface(&gOriginalDepthStencil);

    if (!doRendering) [[unlikely]] {
        return;
    }

    if (gHMD) [[likely]] {
        // UpdateVRPoses should be called as close to rendering as possible
        UpdateVRPoses();

        if (gDriving) {
            RenderVREye(p, LeftEye);
            RenderVREye(p, RightEye);
            PrepareVRRendering(gD3Ddev, Overlay);
        } else {
            // We are not driving, render the scene into a plane
            gVRRenderTarget = std::nullopt;
            if (!IsLoadingBTBStage()) {
                // We cannot render the menu while BTB stage is loading
                // Otherwise it will go into a deadlock of some sort and won't ever finish loading
                PrepareVRRendering(gD3Ddev, Menu);
            }
            hooks::render.call(p);
        }
    } else {
        hooks::render.call(p);
    }

    // Reset VR eye information so that the rest of the shaders won't use the VR projection.
    // It's necessary for rendering the menu elements correctly.
    gVRRenderTarget = std::nullopt;
}

HRESULT __stdcall DXHook_Present(IDirect3DDevice9* This, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion)
{
    if (gHMD) [[likely]] {
        auto current2DRenderTarget = gDriving ? Overlay : Menu;
        if (!IsLoadingBTBStage() || gDriving) [[likely]] {
            FinishVRRendering(gD3Ddev, current2DRenderTarget);
            RenderVROverlay(current2DRenderTarget, !gDriving);
        }
        SubmitFramesToHMD();
    }

    gD3Ddev->SetRenderTarget(0, gOriginalScreenTgt);
    gD3Ddev->SetDepthStencilSurface(gOriginalDepthStencil);
    gOriginalScreenTgt->Release();
    gOriginalDepthStencil->Release();

    return hooks::present.call(This, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT __stdcall DXHook_SetVertexShaderConstantF(IDirect3DDevice9* This, UINT StartRegister, const float* pConstantData, UINT Vector4fCount)
{
    auto shouldPatchShader = true;
    if (gBTBTrackStatus && *gBTBTrackStatus == 1) {
        // While rendering a BTB stage, patch only the original shaders.
        // If we touch BTB shaders, bad things will happen.
        IDirect3DVertexShader9* shader;
        This->GetVertexShader(&shader);
        shouldPatchShader = std::find(gOriginalShaders.cbegin(), gOriginalShaders.cend(), shader) != gOriginalShaders.end();
    }

    if (shouldPatchShader && gVRRenderTarget && Vector4fCount == 4 && StartRegister == 0) {
        // MV = P^-1 * MVP
        // MVP[VRRenderTarget] = P[VRRenderTarget] * MV
        // For some reason the Z-axis is pointing to the wrong direction, so it is flipped here as well

        auto orig = glm::transpose(M4FromShaderConstantPtr(pConstantData));
        auto mv = shader::gCurrentProjectionMatrixInverse * orig;
        auto mvp = glm::transpose(gProjection[gVRRenderTarget.value()] * gEyePos[gVRRenderTarget.value()] * gHMDPose * gFlipZMatrix * mv);
        return hooks::setvertexshaderconstantf.call(This, StartRegister, glm::value_ptr(mvp), Vector4fCount);
    }
    return hooks::setvertexshaderconstantf.call(This, StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall DXHook_SetTransform(IDirect3DDevice9* This, D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix)
{
    if (gVRRenderTarget && State == D3DTS_PROJECTION) {
        shader::gCurrentProjectionMatrix = M4FromD3D(*pMatrix);
        shader::gCurrentProjectionMatrixInverse = glm::inverse(shader::gCurrentProjectionMatrix);
        fixedfunction::gCurrentProjectionMatrix = D3DFromM4(gProjection[gVRRenderTarget.value()]);
        return hooks::settransform.call(This, State, &fixedfunction::gCurrentProjectionMatrix);
    } else if (gVRRenderTarget && State == D3DTS_VIEW) {
        fixedfunction::gCurrentViewMatrix = D3DFromM4(gEyePos[gVRRenderTarget.value()] * gHMDPose * gFlipZMatrix * M4FromD3D(*pMatrix));
        return hooks::settransform.call(This, State, &fixedfunction::gCurrentViewMatrix);
    }

    return hooks::settransform.call(This, State, pMatrix);
}

HRESULT __stdcall DXHook_CreateDevice(
    IDirect3D9* This,
    UINT Adapter,
    D3DDEVTYPE DeviceType,
    HWND hFocusWindow,
    DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS* pPresentationParameters,
    IDirect3DDevice9** ppReturnedDeviceInterface)
{
    IDirect3DDevice9* dev = nullptr;
    auto ret = hooks::createdevice.call(This, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, &dev);
    if (FAILED(ret)) {
        Dbg("D3D initialization failed: CreateDevice");
        return ret;
    }
    *ppReturnedDeviceInterface = dev;

    auto devvtbl = GetVtable<IDirect3DDevice9Vtbl>(dev);
    hooks::setvertexshaderconstantf = Hook(devvtbl->SetVertexShaderConstantF, DXHook_SetVertexShaderConstantF);
    hooks::settransform = Hook(devvtbl->SetTransform, DXHook_SetTransform);
    hooks::present = Hook(devvtbl->Present, DXHook_Present);
    hooks::createvertexshader = Hook(devvtbl->CreateVertexShader, DXHook_CreateVertexShader);

    gD3Ddev = dev;

    RECT winBounds;
    GetWindowRect(hFocusWindow, &winBounds);

    InitVR(dev, gCfg, &gD3DVR, winBounds.right, winBounds.bottom);

    // Initialize this pointer here, as it's too early to do this in openRBRVR constructor
    auto rxHandle = GetModuleHandle("Plugins\\rbr_rx.dll");
    if (rxHandle) {
        auto rxAddr = reinterpret_cast<uintptr_t>(rxHandle);
        gBTBTrackStatus = reinterpret_cast<uint8_t*>(rxAddr + RBRRXTrackStatusOffset);
    }

    return ret;
}

IDirect3D9* __stdcall DXHook_Direct3DCreate9(UINT SDKVersion)
{
    auto d3d = hooks::create.call(SDKVersion);
    auto d3dvtbl = GetVtable<IDirect3D9Vtbl>(d3d);
    hooks::createdevice = Hook(d3dvtbl->CreateDevice, DXHook_CreateDevice);
    return d3d;
}

openRBRVR::openRBRVR(IRBRGame* g)
    : game(g)
    , menuIdx(0)
    , menuPage(0)
    , menuScroll(0)
{
    Dbg("Hooking DirectX");

    auto d3ddll = GetModuleHandle("d3d9.dll");
    if (!d3ddll) {
        Dbg("failed to get handle to d3d9.dll\n");
        return;
    }
    auto d3dcreate = reinterpret_cast<decltype(&Direct3DCreate9)>(GetProcAddress(d3ddll, "Direct3DCreate9"));
    if (!d3dcreate) {
        Dbg("failed to find address to Direct3DCreate9\n");
        return;
    }

    hooks::create = Hook(d3dcreate, DXHook_Direct3DCreate9);
    hooks::render = Hook(*reinterpret_cast<decltype(RBRHook_Render)*>(RBRRenderFunctionAddr), RBRHook_Render);

    gCfg = Config::fromFile("Plugins\\openRBRVR.ini");
}

openRBRVR::~openRBRVR()
{
    ShutdownVR();
}

void openRBRVR::HandleFrontEndEvents(char txtKeyboard, bool bUp, bool bDown, bool bLeft, bool bRight, bool bSelect)
{
    if (menuPage == 0) {
        if (bDown) {
            (++menuIdx) %= menuItems;
        } else if (bUp) {
            (--menuIdx) %= menuItems;
        } else if (bSelect) {
            switch (menuIdx) {
            case 0: {
                vr::VRChaperone()->ResetZeroPose(vr::ETrackingUniverseOrigin::TrackingUniverseSeated);
                break;
            }
            case 1: {
                menuPage = 1;
                break;
            }
            }
        }
    } else if (menuPage == 1) {
        if (bSelect || bLeft) {
            menuPage = 0;
        } else if (bDown) {
            if (menuScroll < gLicenseInformation.size()) {
                menuScroll++;
            }
        } else if (bUp) {
            if (menuScroll > 0) {
                menuScroll--;
            }
        }
    }
}

void openRBRVR::DrawFrontEndPage()
{
    constexpr auto menuItemsStartPos = 70.0f;
    constexpr auto rowHeight = 21.0f;
    constexpr auto licenseRowHeight = 14.0f;

    switch (menuPage) {
    case 0: {
        game->DrawBlackOut(520.0f, 0.0f, 190.0f, 500.0f);
        game->DrawSelection(0.0f, menuItemsStartPos - 2.0f + (static_cast<float>(menuIdx) * rowHeight), 440.0f);

        game->SetFont(IRBRGame::EFonts::FONT_BIG);
        game->SetMenuColor(IRBRGame::EMenuColors::MENU_HEADING);
        game->WriteText(65.0f, 49.0f, "openRBRVR");

        game->SetMenuColor(IRBRGame::EMenuColors::MENU_TEXT);
        game->WriteText(65.0f, menuItemsStartPos, "Recenter view");
        game->WriteText(65.0f, menuItemsStartPos + rowHeight, "Licenses");

        game->WriteText(65.0f, menuItemsStartPos + rowHeight * 3, "Configuration:");
        game->SetFont(IRBRGame::EFonts::FONT_SMALL);
        game->SetColor(0.7f, 0.7f, 0.7f, 1.0f);
        game->WriteText(70.0f, menuItemsStartPos + rowHeight * 4, std::format("Menu size: {:.2f}", gCfg.menuSize).c_str());
        game->WriteText(70.0f, menuItemsStartPos + rowHeight * 5, std::format("Overlay size: {:.2f}", gCfg.overlaySize).c_str());
        game->WriteText(70.0f, menuItemsStartPos + rowHeight * 6, std::format("Supersampling: {:.2f}", gCfg.superSampling).c_str());

        game->SetMenuColor(IRBRGame::EMenuColors::MENU_TEXT);
        game->WriteText(10.0f, 500.0 - rowHeight * 2, "https://github.com/Detegr/openRBRVR");
        break;
    }
    case 1: {
        game->SetFont(IRBRGame::EFonts::FONT_SMALL);
        game->DrawBlackOut(520.0f, 0.0f, 190.0f, 500.0f);
        game->SetFont(IRBRGame::EFonts::FONT_BIG);
        game->SetMenuColor(IRBRGame::EMenuColors::MENU_HEADING);
        game->WriteText(65.0f, 10.0f, "openRBRVR licenses");
        game->SetMenuColor(IRBRGame::EMenuColors::MENU_TEXT);
        game->WriteText(10.0f, 10.0f + rowHeight, "Press enter or left to go back");
        game->SetFont(IRBRGame::EFonts::FONT_SMALL);
        uint32_t i = 0;
        for (auto& row : gLicenseInformation) {
            if (i++ < menuScroll)
                continue;
            game->WriteText(10.0f, 10.0f + rowHeight * 2 + (licenseRowHeight * (i - menuScroll)), row);
        }
        break;
    }
    }
}
