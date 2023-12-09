#include <MinHook.h>
#include <d3d9.h>
#include <format>
#include <optional>
#include <ranges>
#include <vector>

#include "Config.hpp"
#include "D3D.hpp"
#include "Hook.hpp"
#include "Licenses.hpp"
#include "Quaternion.hpp"
#include "Util.hpp"
#include "VR.hpp"
#include "Version.hpp"
#include "Vertex.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "openRBRVR.hpp"

IRBRGame* gGame;

static IDirect3DDevice9* gD3Ddev = nullptr;
Config gCfg;
static Config gSavedCfg;

namespace shader {
    static M4 gCurrentProjectionMatrix;
    static M4 gCurrentProjectionMatrixInverse;
    static M4 gCurrentViewMatrix;
    static M4 gCurrentViewMatrixInverse;
}

namespace fixedfunction {
    static D3DMATRIX gCurrentProjectionMatrix;
    static M4 gCurrentProjectionMatrixInverse;
    static D3DMATRIX gCurrentViewMatrix;
}

static uintptr_t GetRBRBaseAddress()
{
    // If ASLR is enabled, the base address is randomized
    static uintptr_t addr;
    addr = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));
    if (!addr) {
        Dbg("Could not retrieve RBR base address, this may be bad.");
    }
    return addr;
}

static uintptr_t GetRBRAddress(uintptr_t target)
{
    constexpr uintptr_t RBRAbsoluteLoadAddr = 0x400000;
    return GetRBRBaseAddress() + target - RBRAbsoluteLoadAddr;
}

static uintptr_t RBRTrackIdAddr = GetRBRAddress(0x1660804);
static uintptr_t RBRRenderFunctionAddr = GetRBRAddress(0x47E1E0);
static uintptr_t RBRCarQuatPtrAddr = GetRBRAddress(0x8EF660);
void __fastcall RBRHook_Render(void* p);

namespace hooks {
    Hook<decltype(&Direct3DCreate9)> create;
    Hook<decltype(IDirect3D9Vtbl::CreateDevice)> createdevice;
    Hook<decltype(IDirect3DDevice9Vtbl::SetVertexShaderConstantF)> setvertexshaderconstantf;
    Hook<decltype(IDirect3DDevice9Vtbl::SetTransform)> settransform;
    Hook<decltype(IDirect3DDevice9Vtbl::Present)> present;
    Hook<decltype(&RBRHook_Render)> render;
    Hook<decltype(IDirect3DDevice9Vtbl::CreateVertexShader)> createvertexshader;
    Hook<decltype(IDirect3DDevice9Vtbl::SetRenderTarget)> btbsetrendertarget;
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    MH_Initialize();
    return TRUE;
}

static std::optional<RenderTarget> gVRRenderTarget;
static IDirect3DSurface9 *gOriginalScreenTgt, *gOriginalDepthStencil;
static bool gDriving;
static bool gRender3d;
static uint32_t gGameMode;
static std::vector<IDirect3DVertexShader9*> gOriginalShaders;
static std::vector<IDirect3DVertexShader9*> gCockpitShaders;
static Quaternion* gCarQuat;
static M4 gLockToHorizonMatrix = glm::identity<M4>();

constexpr uintptr_t RBRRXDeviceVtblOffset = 0x4f56c;
constexpr uintptr_t RBRRXTrackStatusOffset = 0x608d0;
static volatile uint8_t* gBTBTrackStatus;
static bool IsRBRRXLoaded()
{
    return reinterpret_cast<uintptr_t>(gBTBTrackStatus) != RBRRXTrackStatusOffset;
}

static bool IsRBRHUDLoaded()
{
    static bool triedToObtain;
    static bool isLoaded;

    if (!triedToObtain) {
        auto hudHandle = GetModuleHandle("Plugins\\RBRHUD.dll");
        isLoaded = (hudHandle != nullptr);
    }
    return isLoaded;
}

static std::chrono::steady_clock::time_point gFrameStart;

static bool IsLoadingBTBStage()
{
    return gBTBTrackStatus && *gBTBTrackStatus == 1 && gGameMode == 5;
}

HRESULT __stdcall DXHook_CreateVertexShader(IDirect3DDevice9* This, const DWORD* pFunction, IDirect3DVertexShader9** ppShader)
{
    static int i = 0;
    auto ret = hooks::createvertexshader.call(This, pFunction, ppShader);
    if (i < 40) {
        switch (i) {
            case 2:
            case 5:
            case 6:
            case 7:
                // These shaders are used to draw the cockpit of the car
                // We'll save them to draw them with different Z plane
                // clipping values
                gCockpitShaders.push_back(*ppShader);
                [[fallthrough]];
            default:
                // These are the original shaders for RBR that need
                // to be patched with the VR projection.
                gOriginalShaders.push_back(*ppShader);
        }
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
    auto size = renderTarget2D == Menu ? gCfg.menuSize : gCfg.overlaySize;
    auto translation = renderTarget2D == Overlay ? gCfg.overlayTranslation : glm::vec3 { 0.0f, 0.0f, 0.0f };
    auto horizLock = renderTarget2D == Overlay ? std::make_optional(gLockToHorizonMatrix) : std::nullopt;

    PrepareVRRendering(gD3Ddev, LeftEye, clear);
    RenderMenuQuad(gD3Ddev, LeftEye, renderTarget2D, size, translation, horizLock);
    FinishVRRendering(gD3Ddev, LeftEye);

    PrepareVRRendering(gD3Ddev, RightEye, clear);
    RenderMenuQuad(gD3Ddev, RightEye, renderTarget2D, size, translation, horizLock);
    FinishVRRendering(gD3Ddev, RightEye);
}

// RBR 3D scene draw function is rerouted here
void __fastcall RBRHook_Render(void* p)
{
    auto ptr = reinterpret_cast<uintptr_t>(p);
    auto doRendering = *reinterpret_cast<uint32_t*>(ptr + 0x720) == 0;
    gGameMode = *reinterpret_cast<uint32_t*>(ptr + 0x728);
    gDriving = gGameMode == 1;
    gRender3d = gDriving || (gCfg.renderMainMenu3d && gGameMode == 3) || (gCfg.renderPauseMenu3d && gGameMode == 2) || (gCfg.renderPreStage3d && gGameMode == 10);

    gD3Ddev->GetRenderTarget(0, &gOriginalScreenTgt);
    gD3Ddev->GetDepthStencilSurface(&gOriginalDepthStencil);

    if (!doRendering) [[unlikely]] {
        return;
    }

    if (gHMD) [[likely]] {
        if (gDriving && (gCfg.lockToHorizon != Config::HorizonLock::LOCK_NONE) && !gCarQuat) [[unlikely]] {
            uintptr_t p = *reinterpret_cast<uintptr_t*>(RBRCarQuatPtrAddr) + 0x100;
            gCarQuat = reinterpret_cast<Quaternion*>(p);
        } else if (gCarQuat && !gDriving) [[unlikely]] {
            gCarQuat = nullptr;
            gLockToHorizonMatrix = glm::identity<M4>();
        }

        // UpdateVRPoses should be called as close to rendering as possible
        UpdateVRPoses(gCarQuat, gCfg.lockToHorizon, &gLockToHorizonMatrix);

        if (gCfg.debug) [[unlikely]] {
            gFrameStart = std::chrono::steady_clock::now();
        }

        if (gRender3d) [[likely]] {
            RenderVREye(p, LeftEye);
            RenderVREye(p, RightEye);
            PrepareVRRendering(gD3Ddev, Overlay);
        } else {
            // We are not driving, render the scene into a plane
            gVRRenderTarget = std::nullopt;
            auto shouldSwapRenderTarget = !(IsLoadingBTBStage() && !gCfg.drawLoadingScreen);
            if (shouldSwapRenderTarget) {
                PrepareVRRendering(gD3Ddev, Menu);
            }

            auto shouldRender = !(gGameMode == 5 && !gCfg.drawLoadingScreen);
            if (shouldRender) {
                hooks::render.call(p);
            }
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
        auto current2DRenderTarget = gRender3d ? Overlay : Menu;
        auto shouldRender = !(IsLoadingBTBStage() && !gCfg.drawLoadingScreen);
        if (shouldRender) [[likely]] {
            FinishVRRendering(gD3Ddev, current2DRenderTarget);
            RenderVROverlay(current2DRenderTarget, !gRender3d);
        }
        SubmitFramesToHMD(gD3Ddev);
    }

    gD3Ddev->SetRenderTarget(0, gOriginalScreenTgt);
    gD3Ddev->SetDepthStencilSurface(gOriginalDepthStencil);
    gOriginalScreenTgt->Release();
    gOriginalDepthStencil->Release();

    if (gHMD && gCfg.drawCompanionWindow) {
        RenderCompanionWindowFromRenderTarget(This, gRender3d ? LeftEye : Menu);
    }

    auto ret = hooks::present.call(This, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

    if (gCfg.debug && gHMD) [[unlikely]] {
        auto frameEnd = std::chrono::steady_clock::now();
        auto frameTime = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - gFrameStart);

        auto t = GetFrameTiming();
        static uint32_t totalDroppedFrames = 0;
        static uint32_t totalMispresentedFrames = 0;
        totalDroppedFrames += t.droppedFrames;
        totalMispresentedFrames += t.mispresentedFrames;

        gGame->SetColor(1, 0, 1, 1.0);
        gGame->SetFont(IRBRGame::EFonts::FONT_DEBUG);
        gGame->WriteText(0, 18 * 0, std::format("openRBRVR {}", VERSION_STR).c_str());
        gGame->WriteText(0, 18 * 1,
            std::format("CPU: render time: {:.2f}ms, present: {:.2f}ms, waitforpresent: {:.2f}ms",
                frameTime.count() / 1000.0,
                t.cpuPresentCall,
                t.cpuWaitForPresent)
                .c_str());
        gGame->WriteText(0, 18 * 2,
            std::format("GPU: presubmit {:.2f}ms, postSubmit: {:.2f}ms, total: {:.2f}ms",
                t.gpuPreSubmit,
                t.gpuPostSubmit,
                t.gpuTotal)
                .c_str());
        gGame->WriteText(0, 18 * 3,
            std::format("Compositor: CPU: {:.2f}ms, GPU: {:.2f}ms, submit: {:.2f}ms",
                t.compositorCpu,
                t.compositorGpu,
                t.compositorSubmitFrame)
                .c_str());
        gGame->WriteText(0, 18 * 4,
            std::format("Total mispresented frames: {}, Total dropped frames: {}, Reprojection flags: {:X}",
                totalMispresentedFrames,
                totalDroppedFrames,
                t.reprojectionFlags)
                .c_str());
        gGame->WriteText(0, 18 * 5, std::format("Mods: {} {}", IsRBRRXLoaded() ? "RBRRX" : "", IsRBRHUDLoaded() ? "RBRHUD" : "").c_str());
        const auto& [lw, lh] = GetRenderResolution(LeftEye);
        const auto& [rw, rh] = GetRenderResolution(RightEye);
        gGame->WriteText(0, 18 * 6, std::format("Render resolution: {}x{} (left), {}x{} (right)", lw, lh, rw, rh).c_str());
    }

    return ret;
}

HRESULT __stdcall DXHook_SetVertexShaderConstantF(IDirect3DDevice9* This, UINT StartRegister, const float* pConstantData, UINT Vector4fCount)
{
    IDirect3DVertexShader9* shader;
    This->GetVertexShader(&shader);

    auto isOriginalShader = true;
    if (gBTBTrackStatus && *gBTBTrackStatus == 1) {
        isOriginalShader = std::find(gOriginalShaders.cbegin(), gOriginalShaders.cend(), shader) != gOriginalShaders.end();
    }

    if (gVRRenderTarget && Vector4fCount == 4) {
        // The cockpit of the car is drawn with a projection matrix that has smaller near Z value
        // Otherwise with wide FoV headsets part of the car will be clipped out.
        // Separate projection matrix is used because if the small near Z value is used for all shaders
        // it will cause Z fighting (flickering) in the trackside objects. With this we get best of both worlds.
        auto isDrawingCockpit = std::find(gCockpitShaders.cbegin(), gCockpitShaders.cend(), shader) != gCockpitShaders.end();

        if (StartRegister == 0) {
            const auto orig = glm::transpose(M4FromShaderConstantPtr(pConstantData));

            if (isOriginalShader) {
                const auto& projection = isDrawingCockpit ? gCockpitProjection : gProjection;

                // MVP matrix
                // MV = P^-1 * MVP
                // MVP[VRRenderTarget] = P[VRRenderTarget] * MV
                const auto mv = shader::gCurrentProjectionMatrixInverse * orig;

                // For some reason the Z-axis is pointing to the wrong direction, so it is flipped here as well
                const auto mvp = glm::transpose(projection[gVRRenderTarget.value()] * gEyePos[gVRRenderTarget.value()] * gHMDPose * gFlipZMatrix * gLockToHorizonMatrix * mv);
                return hooks::setvertexshaderconstantf.call(This, StartRegister, glm::value_ptr(mvp), Vector4fCount);
            } else {
                // We're drawing BTB stage geometry. Here we want to use the gProjection instead of gCockpitProjection. The shader
                // gets this value from the fixed function projection matrix, which is set to gCockpitProjection in order to draw
                // some of the car's internal geometry with correct Z-values. This is done this way because we can't detect when
                // we're drawing those car parts, so it needs to be the default. We can detect BTB stage drawing (that is now),
                // so we're patching the projection matrix here from gCockpitProjection to gProjection.
                // Everything else apart from the projection is already correct in the matrix that is supplied to this function,
                // so the calculation here is simpler than in the case above.
                const auto mv = fixedfunction::gCurrentProjectionMatrixInverse * orig;
                const auto mvp = glm::transpose(gProjection[gVRRenderTarget.value()] * mv);
                return hooks::setvertexshaderconstantf.call(This, StartRegister, glm::value_ptr(mvp), Vector4fCount);
            }
        } else if (isOriginalShader && StartRegister == 20) {
            // Sky/fog
            // It seems this parameter contains the orientation of the car and the
            // skybox and fog is rendered correctly only in the direction where the car
            // points at. By rotating this with the HMD's rotation, the skybox and possible
            // fog is rendered correctly.
            const auto orig = glm::transpose(M4FromShaderConstantPtr(pConstantData));
            glm::vec4 perspective;
            glm::vec3 scale, translation, skew;
            glm::quat orientation;
            glm::decompose(gHMDPose, scale, orientation, translation, skew, perspective);
            const auto rotation = glm::mat4_cast(glm::conjugate(orientation));

            const auto m = glm::transpose(rotation * orig);
            return hooks::setvertexshaderconstantf.call(This, StartRegister, glm::value_ptr(m), Vector4fCount);
        }
    }
    return hooks::setvertexshaderconstantf.call(This, StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall DXHook_SetTransform(IDirect3DDevice9* This, D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix)
{
    if (gVRRenderTarget && State == D3DTS_PROJECTION) {
        shader::gCurrentProjectionMatrix = M4FromD3D(*pMatrix);
        shader::gCurrentProjectionMatrixInverse = glm::inverse(shader::gCurrentProjectionMatrix);
        // Use cockpit Z-values here, as fixed function stuff is used for some parts of the car internals only.
        // BTB also uses this matrix, but it is patched to use the stage Z-values later
        fixedfunction::gCurrentProjectionMatrix = D3DFromM4(gCockpitProjection[gVRRenderTarget.value()]);
        fixedfunction::gCurrentProjectionMatrixInverse = glm::inverse(gCockpitProjection[gVRRenderTarget.value()]);
        return hooks::settransform.call(This, State, &fixedfunction::gCurrentProjectionMatrix);
    } else if (gVRRenderTarget && State == D3DTS_VIEW) {
        fixedfunction::gCurrentViewMatrix = D3DFromM4(gEyePos[gVRRenderTarget.value()] * gHMDPose * gFlipZMatrix * gLockToHorizonMatrix * M4FromD3D(*pMatrix));
        return hooks::settransform.call(This, State, &fixedfunction::gCurrentViewMatrix);
    }

    return hooks::settransform.call(This, State, pMatrix);
}

HRESULT __stdcall BTB_SetRenderTarget(IDirect3DDevice9* This, DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget)
{
    // This was found purely by luck after testing all kinds of things.
    // For some reason, if this call is called with the original This pointer (from RBRRX)
    // plugins switching the render target (i.e. RBRHUD) will cause the stage geometry
    // to not be rendered at all. Routing the call to the D3D device created by openRBRVR,
    // it seems to work correctly.
    return gD3Ddev->SetRenderTarget(RenderTargetIndex, pRenderTarget);
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

        IDirect3DDevice9Vtbl* rbrrxdev = reinterpret_cast<IDirect3DDevice9Vtbl*>(rxAddr + RBRRXDeviceVtblOffset);
        hooks::btbsetrendertarget = Hook(rbrrxdev->SetRenderTarget, BTB_SetRenderTarget);
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
    gGame = g;
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
    gSavedCfg = gCfg;
}

openRBRVR::~openRBRVR()
{
    ShutdownVR();
}

static Config::HorizonLock ChangeHorizonLock(Config::HorizonLock lock, bool forward)
{
    switch (lock) {
        case Config::HorizonLock::LOCK_NONE:
            return forward ? Config::HorizonLock::LOCK_ROLL : static_cast<Config::HorizonLock>((Config::HorizonLock::LOCK_ROLL | Config::HorizonLock::LOCK_PITCH));
        case Config::HorizonLock::LOCK_ROLL:
            return forward ? Config::HorizonLock::LOCK_PITCH : Config::HorizonLock::LOCK_NONE;
        case Config::HorizonLock::LOCK_PITCH:
            return forward ? static_cast<Config::HorizonLock>((Config::HorizonLock::LOCK_ROLL | Config::HorizonLock::LOCK_PITCH)) : Config::HorizonLock::LOCK_ROLL;
        case (Config::HorizonLock::LOCK_ROLL | Config::HorizonLock::LOCK_PITCH):
            return forward ? Config::HorizonLock::LOCK_NONE : Config::HorizonLock::LOCK_PITCH;
        default:
            return Config::HorizonLock::LOCK_NONE;
    }
}

enum MenuItems {
    RECENTER_VIEW = 0,
    TOGGLE_DEBUG = 1,
    COMPANION_WINDOW = 2,
    LOADING_SCREEN = 3,
    HORIZON = 4,
    RENDER_3D = 5,
    LICENSES = 6,
    SAVE = 7,
    MENU_ITEM_COUNT = 8,
};

void ChangeRenderIn3dSettings(bool forward) {
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

void openRBRVR::HandleFrontEndEvents(char txtKeyboard, bool bUp, bool bDown, bool bLeft, bool bRight, bool bSelect)
{
    if (menuPage == 0) {
        if (bDown) {
            (++menuIdx) %= MENU_ITEM_COUNT;
            if (menuIdx == SAVE && gCfg == gSavedCfg) {
                (++menuIdx) %= MENU_ITEM_COUNT;
            }
        } else if (bUp) {
            if (--menuIdx < 0) {
                menuIdx = MENU_ITEM_COUNT - 2;
            }
        } else if (bSelect) {
            switch (menuIdx) {
                case RECENTER_VIEW: {
                    vr::VRChaperone()->ResetZeroPose(vr::ETrackingUniverseOrigin::TrackingUniverseSeated);
                    break;
                }
                case TOGGLE_DEBUG: {
                    gCfg.debug = !gCfg.debug;
                    break;
                }
                case COMPANION_WINDOW: {
                    gCfg.drawCompanionWindow = !gCfg.drawCompanionWindow;
                    break;
                }
                case LOADING_SCREEN: {
                    gCfg.drawLoadingScreen = !gCfg.drawLoadingScreen;
                    break;
                }
                case HORIZON: {
                    gCfg.lockToHorizon = ChangeHorizonLock(gCfg.lockToHorizon, true);
                    break;
                }
                case RENDER_3D: {
                    ChangeRenderIn3dSettings(true);
                    break;
                }
                case LICENSES: {
                    menuPage = 1;
                    break;
                }
                case SAVE: {
                    if (gCfg.Write("Plugins\\openRBRVR.ini")) {
                        gSavedCfg = gCfg;
                    }
                    break;
                }
                default:
                    break;
            }
        } else if (bLeft) {
            switch (menuIdx) {
                case TOGGLE_DEBUG: {
                    gCfg.debug = !gCfg.debug;
                    break;
                }
                case COMPANION_WINDOW: {
                    gCfg.drawCompanionWindow = !gCfg.drawCompanionWindow;
                    break;
                }
                case LOADING_SCREEN: {
                    gCfg.drawLoadingScreen = !gCfg.drawLoadingScreen;
                    break;
                }
                case HORIZON: {
                    gCfg.lockToHorizon = ChangeHorizonLock(gCfg.lockToHorizon, false);
                    break;
                }
                case RENDER_3D: {
                    ChangeRenderIn3dSettings(false);
                    break;
                }
            }
        } else if (bRight) {
            switch (menuIdx) {
                case TOGGLE_DEBUG: {
                    gCfg.debug = !gCfg.debug;
                    break;
                }
                case COMPANION_WINDOW: {
                    gCfg.drawCompanionWindow = !gCfg.drawCompanionWindow;
                    break;
                }
                case LOADING_SCREEN: {
                    gCfg.drawLoadingScreen = !gCfg.drawLoadingScreen;
                    break;
                }
                case HORIZON: {
                    gCfg.lockToHorizon = ChangeHorizonLock(gCfg.lockToHorizon, true);
                    break;
                }
                case RENDER_3D: {
                    ChangeRenderIn3dSettings(true);
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

struct MenuEntry {
    std::string text;
    std::optional<IRBRGame::EFonts> font;
    std::optional<IRBRGame::EMenuColors> menuColor;
    std::optional<std::tuple<float, float, float, float>> color;
    std::optional<std::tuple<float, float>> position;
};

void openRBRVR::DrawMenuEntries(const std::ranges::forward_range auto& entries, float rowHeight)
{
    auto i = 0;
    auto x = 0.0f;
    auto y = 0.0f;
    for (const auto& entry : entries) {
        if (entry.font) {
            game->SetFont(entry.font.value());
        }
        if (entry.menuColor) {
            game->SetMenuColor(entry.menuColor.value());
        } else if (entry.color) {
            auto [r, g, b, a] = entry.color.value();
            game->SetColor(r, g, b, a);
        }
        if (entry.position) {
            auto [newx, newy] = entry.position.value();
            x = newx;
            y = newy;
        }
        game->WriteText(x, y, entry.text.c_str());
        y += rowHeight;
    }
}

void openRBRVR::DrawFrontEndPage()
{
    constexpr auto menuItemsStartPos = 70.0f;
    constexpr auto rowHeight = 21.0f;
    constexpr auto licenseRowHeight = 14.0f;
    constexpr auto configurationTextColor = std::make_tuple(0.7f, 0.7f, 0.7f, 1.0f);

    MenuEntry renderResolution;

    if (gHMD) {
        const auto& [lw, lh] = GetRenderResolution(LeftEye);
        const auto& [rw, rh] = GetRenderResolution(RightEye);
        renderResolution = { std::format("Render resolution: {}x{} (left), {}x{} (right)", lw, lh, rw, rh) };
    } else {
        renderResolution = { "VR mode not initialized" };
    }

    switch (menuPage) {
        case 0: {
            game->DrawBlackOut(520.0f, 0.0f, 190.0f, 500.0f);
            game->DrawSelection(0.0f, menuItemsStartPos - 2.0f + (static_cast<float>(menuIdx) * rowHeight), 440.0f);

            // clang-format off
            DrawMenuEntries(std::to_array<MenuEntry>({
                {"openRBRVR", IRBRGame::EFonts::FONT_BIG, IRBRGame::EMenuColors::MENU_HEADING, std::nullopt, std::make_tuple(65.0f, 49.0f)},
                {"Recenter view", std::nullopt, IRBRGame::EMenuColors::MENU_TEXT, std::nullopt, std::make_tuple(65.0f, menuItemsStartPos)},
                {std::format("Debug information: {}", gCfg.debug ? "ON" : "OFF")},
                {std::format("Draw desktop window: {}", gCfg.drawCompanionWindow ? "ON" : "OFF")},
                {std::format("Draw loading screen: {}", gCfg.drawLoadingScreen ? "ON" : "OFF")},
                {std::format("Lock horizon: {}", GetHorizonLockStr())},
                {std::format("Render in 3D: [{}] pause menu, [{}] pre-stage, [{}] main menu",
                    gCfg.renderPauseMenu3d ? "x" : " ",
                    gCfg.renderPreStage3d ? "x" : " ",
                    gCfg.renderMainMenu3d ? "x" : " ")},
                {"Licenses"},
                {.text = "Save the current config to openRBRVR.ini", .color = (gCfg == gSavedCfg) ? std::make_tuple(0.5f, 0.5f, 0.5f, 1.0f) : std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f)},
                {""},
                {"Configuration:", IRBRGame::EFonts::FONT_BIG, std::nullopt, configurationTextColor},
                {std::format("Menu size: {:.2f}", gCfg.menuSize), IRBRGame::EFonts::FONT_SMALL},
                {std::format("Overlay size: {:.2f}", gCfg.overlaySize)},
                {std::format("Supersampling: {:.2f}", gCfg.superSampling)},
                renderResolution,
                {"https://github.com/Detegr/openRBRVR", std::nullopt, std::nullopt, std::nullopt, std::make_tuple(10.0f, 500.0f - rowHeight*2)},
            }));
            //clang-format on
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
        }
    }
}
