#include <MinHook.h>
#include <d3d9.h>
#include <format>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "Config.hpp"
#include "D3D.hpp"
#include "Hook.hpp"
#include "Licenses.hpp"
#include "Menu.hpp"
#include "OpenVR.hpp"
#include "OpenXR.hpp"
#include "Quaternion.hpp"
#include "Util.hpp"
#include "VR.hpp"
#include "Version.hpp"
#include "Vertex.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "openRBRVR.hpp"

IRBRGame* gGame;

std::unique_ptr<VRInterface> gVR;
Config gCfg, gSavedCfg;

static IDirect3DDevice9* gD3Ddev = nullptr;

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
static uintptr_t RBRCarInfoAddr = GetRBRAddress(0x165FC68);
void __fastcall RBRHook_Render(void* p);
uint32_t __stdcall RBRHook_LoadTexture(void* p, const char* texName, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t g, uint32_t h, uint32_t i, uint32_t j, uint32_t k, IDirect3DTexture9** ppTexture);
void __stdcall RBRHook_RenderCar(void* a, void* b);

namespace hooks {
    Hook<decltype(&Direct3DCreate9)> create;
    Hook<decltype(IDirect3D9Vtbl::CreateDevice)> createdevice;
    Hook<decltype(IDirect3DDevice9Vtbl::SetVertexShaderConstantF)> setvertexshaderconstantf;
    Hook<decltype(IDirect3DDevice9Vtbl::SetTransform)> settransform;
    Hook<decltype(IDirect3DDevice9Vtbl::Present)> present;
    Hook<decltype(&RBRHook_Render)> render;
    Hook<decltype(IDirect3DDevice9Vtbl::CreateVertexShader)> createvertexshader;
    Hook<decltype(IDirect3DDevice9Vtbl::SetRenderTarget)> btbsetrendertarget;
    Hook<decltype(&RBRHook_LoadTexture)> loadtexture;
    Hook<decltype(IDirect3DDevice9Vtbl::DrawIndexedPrimitive)> drawindexedprimitive;
    Hook<decltype(&RBRHook_RenderCar)> rendercar;
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    MH_Initialize();
    return TRUE;
}

enum GameMode : uint32_t {
    Driving = 1,
    Pause = 2,
    MainMenu = 3,
    Blackout = 4,
    Loading = 5,
    Exiting = 6,
    Quit = 7,
    Replay = 8,
    End = 9,
    PreStage = 10,
    Starting = 12,
};

static std::optional<RenderTarget> gVRRenderTarget;
static std::optional<RenderTarget> gCurrent2DRenderTarget;
static IDirect3DSurface9 *gOriginalScreenTgt, *gOriginalDepthStencil;
static bool gDriving;
static bool gRender3d;
static GameMode gGameMode;
static GameMode gPrevGameMode;
static std::vector<IDirect3DVertexShader9*> gOriginalShaders;
static std::unordered_map<std::string, IDirect3DTexture9*> gCarTextures;
static Quaternion* gCarQuat;
static uint32_t* gCameraType;
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
    return gBTBTrackStatus && *gBTBTrackStatus == 1 && gGameMode == GameMode::Loading;
}

constexpr static bool IsUsingCockpitCamera()
{
    if (!gCameraType) {
        return false;
    }
    return (*gCameraType >= 3) && (*gCameraType <= 6);
}

static bool IsCarTexture(IDirect3DBaseTexture9* tex)
{
    for (const auto& entry : gCarTextures) {
        if (std::get<1>(entry) == tex) {
            return true;
        }
    }
    return false;
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
    if (gVR->PrepareVRRendering(gD3Ddev, eye, clear)) {
        hooks::render.call(p);
        gVR->FinishVRRendering(gD3Ddev, eye);
    } else {
        Dbg("Failed to set 3D render target");
        gD3Ddev->SetRenderTarget(0, gOriginalScreenTgt);
        gD3Ddev->SetDepthStencilSurface(gOriginalDepthStencil);
    }
    gVRRenderTarget = std::nullopt;
}

// Render `renderTarget2d` on a plane for both eyes
void RenderVROverlay(RenderTarget renderTarget2D, bool clear)
{
    const auto& size = renderTarget2D == Menu ? gCfg.menuSize : gCfg.overlaySize;
    const auto& translation = renderTarget2D == Overlay ? gCfg.overlayTranslation : glm::vec3 { 0.0f, -0.1f, 0.0f };
    const auto& horizLock = renderTarget2D == Overlay ? std::make_optional(gVR->GetHorizonLock()) : std::nullopt;
    const auto projType = gGameMode == GameMode::MainMenu ? Projection::MainMenu : Projection::Cockpit;

    if (gVR->PrepareVRRendering(gD3Ddev, LeftEye, clear)) [[likely]] {
        RenderMenuQuad(gD3Ddev, gVR.get(), LeftEye, renderTarget2D, projType, size, translation, horizLock);
        gVR->FinishVRRendering(gD3Ddev, LeftEye);
    } else {
        Dbg("Failed to render left eye overlay");
    }

    if (gVR->PrepareVRRendering(gD3Ddev, RightEye, clear)) [[likely]] {
        RenderMenuQuad(gD3Ddev, gVR.get(), RightEye, renderTarget2D, projType, size, translation, horizLock);
        gVR->FinishVRRendering(gD3Ddev, RightEye);
    } else {
        Dbg("Failed to render left eye overlay");
    }
}

// RBR 3D scene draw function is rerouted here
void __fastcall RBRHook_Render(void* p)
{
    if (!hooks::rendercar.call || !hooks::loadtexture.call) [[unlikely]] {
        auto hedgehoghandle = reinterpret_cast<uintptr_t>(GetModuleHandle("HedgeHog3D.dll"));
        if (!hedgehoghandle) {
            Dbg("Could not get handle for HedgeHog3D");
        } else {
            if (!hooks::loadtexture.call) {
                hooks::loadtexture = Hook(*reinterpret_cast<decltype(RBRHook_LoadTexture)*>(hedgehoghandle + 0xAEC15), RBRHook_LoadTexture);
            }
            if (!hooks::rendercar.call) {
                hooks::rendercar = Hook(*reinterpret_cast<decltype(RBRHook_RenderCar)*>(hedgehoghandle + 0x7BC60), RBRHook_RenderCar);
            }
        }
    }

    auto ptr = reinterpret_cast<uintptr_t>(p);
    auto doRendering = *reinterpret_cast<uint32_t*>(ptr + 0x720) == 0;
    auto gameMode = *reinterpret_cast<GameMode*>(ptr + 0x728);
    if (gameMode != gGameMode) [[unlikely]] {
        gPrevGameMode = gGameMode;
        gGameMode = gameMode;
    }
    gDriving = gGameMode == Driving;
    gRender3d = gDriving
        || (gCfg.renderMainMenu3d && gGameMode == GameMode::MainMenu)
        || (gCfg.renderPauseMenu3d && gGameMode == GameMode::Pause && gPrevGameMode != GameMode::Replay)
        || (gCfg.renderPauseMenu3d && gGameMode == GameMode::Pause && (gPrevGameMode == GameMode::Replay && gCfg.renderReplays3d))
        || (gCfg.renderPreStage3d && gGameMode == GameMode::PreStage)
        || (gCfg.renderReplays3d && gGameMode == GameMode::Replay);

    if (gD3Ddev->GetRenderTarget(0, &gOriginalScreenTgt) != D3D_OK) [[unlikely]] {
        Dbg("Could not get render original target");
    }
    if (gD3Ddev->GetDepthStencilSurface(&gOriginalDepthStencil) != D3D_OK) [[unlikely]] {
        Dbg("Could not get render original depth stencil surface");
    }

    if (!doRendering) [[unlikely]] {
        return;
    }

    if (gGameMode == GameMode::MainMenu && !gCarTextures.empty()) [[unlikely]] {
        // Clear saved car textures if we're in the menu
        // Not sure if this is needed, but better be safe than sorry,
        // the car textures will be reloaded when loading the stage.
        gCarTextures.clear();
    }

    if (!gCameraType) [[unlikely]] {
        uintptr_t cameraData = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(RBRCarInfoAddr) + 0x758);
        uintptr_t cameraInfo = *reinterpret_cast<uintptr_t*>(cameraData + 0x10);
        gCameraType = reinterpret_cast<uint32_t*>(cameraInfo);
    }

    if (gVR) [[likely]] {
        if (gDriving && (gCfg.lockToHorizon != Config::HorizonLock::LOCK_NONE) && !gCarQuat) [[unlikely]] {
            uintptr_t p = *reinterpret_cast<uintptr_t*>(RBRCarQuatPtrAddr) + 0x100;
            gCarQuat = reinterpret_cast<Quaternion*>(p);
        } else if (gCarQuat && !gDriving) [[unlikely]] {
            gCarQuat = nullptr;
        }

        // UpdateVRPoses should be called as close to rendering as possible
        gVR->UpdateVRPoses(gCarQuat, gCfg.lockToHorizon);

        if (gCfg.debug) {
            gFrameStart = std::chrono::steady_clock::now();
        }

        if (gRender3d) {
            RenderVREye(p, LeftEye);
            RenderVREye(p, RightEye);
            if (gVR->PrepareVRRendering(gD3Ddev, Overlay)) {
                gCurrent2DRenderTarget = Overlay;
            } else {
                Dbg("Failed to set 2D render target");
                gCurrent2DRenderTarget = std::nullopt;
                gD3Ddev->SetRenderTarget(0, gOriginalScreenTgt);
                gD3Ddev->SetDepthStencilSurface(gOriginalDepthStencil);
            }
        } else {
            // We are not driving, render the scene into a plane
            gVRRenderTarget = std::nullopt;
            auto shouldSwapRenderTarget = !(IsLoadingBTBStage() && !gCfg.drawLoadingScreen);
            if (shouldSwapRenderTarget) {
                if (gVR->PrepareVRRendering(gD3Ddev, Menu)) {
                    gCurrent2DRenderTarget = Menu;
                } else {
                    Dbg("Failed to set 2D render target");
                    gCurrent2DRenderTarget = std::nullopt;
                    gD3Ddev->SetRenderTarget(0, gOriginalScreenTgt);
                    gD3Ddev->SetDepthStencilSurface(gOriginalDepthStencil);
                    return;
                }
            }

            auto shouldRender = !(gGameMode == GameMode::Loading && !gCfg.drawLoadingScreen);
            if (shouldRender) {
                hooks::render.call(p);
            }
        }
    } else {
        hooks::render.call(p);
    }
}

HRESULT __stdcall DXHook_Present(IDirect3DDevice9* This, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion)
{
    if (gVR) [[likely]] {
        auto shouldRender = gCurrent2DRenderTarget && !(IsLoadingBTBStage() && !gCfg.drawLoadingScreen);
        if (shouldRender) {
            gVR->FinishVRRendering(gD3Ddev, gCurrent2DRenderTarget.value());
            RenderVROverlay(gCurrent2DRenderTarget.value(), !gRender3d);
        }
        gVR->SubmitFramesToHMD(gD3Ddev);
    }

    if (gD3Ddev->SetRenderTarget(0, gOriginalScreenTgt) != D3D_OK) {
        Dbg("Failed to reset render target to original");
    }
    if (gD3Ddev->SetDepthStencilSurface(gOriginalDepthStencil) != D3D_OK) {
        Dbg("Failed to reset depth stencil surface to original");
    }
    gOriginalScreenTgt->Release();
    gOriginalDepthStencil->Release();

    auto ret = 0;
    if (gCfg.alwaysPresent || !gVR) {
        if (gVR && (gCfg.drawCompanionWindow || !gDriving)) {
            RenderCompanionWindowFromRenderTarget(This, gVR.get(), gRender3d ? LeftEye : Menu);
        }
        ret = hooks::present.call(This, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    }

    if (gCfg.debug && gVR) [[unlikely]] {
        auto frameEnd = std::chrono::steady_clock::now();
        auto frameTime = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - gFrameStart);

        auto t = gVR->GetFrameTiming();
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
        const auto& [lw, lh] = gVR->GetRenderResolution(LeftEye);
        const auto& [rw, rh] = gVR->GetRenderResolution(RightEye);
        gGame->WriteText(0, 18 * 6, std::format("Render resolution: {}x{} (left), {}x{} (right)", lw, lh, rw, rh).c_str());
        gGame->WriteText(0, 18 * 7, std::format("Anti-aliasing: {}x", static_cast<int>(gCfg.msaa)).c_str());
        gGame->WriteText(0, 18 * 8, std::format("Anisotropic filtering: {}x", gCfg.anisotropy).c_str());
        auto i = 9;
    }

    return ret;
}

static bool gRenderingCar;
void __stdcall RBRHook_RenderCar(void* a, void* b)
{
    gRenderingCar = true;
    hooks::rendercar.call(a, b);
    gRenderingCar = false;
}

HRESULT __stdcall DXHook_SetVertexShaderConstantF(IDirect3DDevice9* This, UINT StartRegister, const float* pConstantData, UINT Vector4fCount)
{
    IDirect3DVertexShader9* shader;
    if (auto ret = This->GetVertexShader(&shader); ret != D3D_OK) {
        Dbg("Could not get vertex shader");
        return ret;
    }

    auto isOriginalShader = true;
    if (gBTBTrackStatus && *gBTBTrackStatus == 1) {
        isOriginalShader = std::find(gOriginalShaders.cbegin(), gOriginalShaders.cend(), shader) != gOriginalShaders.end();
    }
    if (shader)
        shader->Release();

    if (isOriginalShader && gVRRenderTarget && Vector4fCount == 4) {
        // The cockpit of the car is drawn with a projection matrix that has smaller near Z value
        // Otherwise with wide FoV headsets part of the car will be clipped out.
        // Separate projection matrix is used because if the small near Z value is used for all shaders
        // it will cause Z fighting (flickering) in the trackside objects. With this we get best of both worlds.
        IDirect3DBaseTexture9* texture;
        if (auto ret = This->GetTexture(0, &texture); ret != D3D_OK) {
            Dbg("Could not get texture");
            return ret;
        }

        auto isRenderingCockpit = IsUsingCockpitCamera() && (gRenderingCar || IsCarTexture(texture));
        if (texture) {
            texture->Release();
        }

        if (StartRegister == 0) {
            const auto orig = glm::transpose(M4FromShaderConstantPtr(pConstantData));
            const auto& projection = gVR->GetProjection(gVRRenderTarget.value(), isRenderingCockpit ? Projection::Cockpit : Projection::Stage);
            const auto& eyepos = gVR->GetEyePos(gVRRenderTarget.value());
            const auto& pose = gVR->GetPose();
            const auto& horizonLock = gVR->GetHorizonLock();

            // MVP matrix
            // MV = P^-1 * MVP
            // MVP[VRRenderTarget] = P[VRRenderTarget] * MV
            const auto mv = shader::gCurrentProjectionMatrixInverse * orig;
            const auto mvp = glm::transpose(projection * eyepos * pose * gFlipZMatrix * horizonLock * mv);
            return hooks::setvertexshaderconstantf.call(This, StartRegister, glm::value_ptr(mvp), Vector4fCount);
        } else if (StartRegister == 20) {
            // Sky/fog
            // It seems this parameter contains the orientation of the car and the
            // skybox and fog is rendered correctly only in the direction where the car
            // points at. By rotating this with the HMD's rotation, the skybox and possible
            // fog is rendered correctly.
            const auto orig = glm::transpose(M4FromShaderConstantPtr(pConstantData));
            glm::vec4 perspective;
            glm::vec3 scale, translation, skew;
            glm::quat orientation;

            // TODO: OpenXR pose
            glm::decompose(gVR->GetPose(), scale, orientation, translation, skew, perspective);
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

        // Use the large space z-value projection matrix by default. The car specific parts
        // are patched to be rendered in gCockpitProjection in DXHook_DrawIndexedPrimitive.
        const auto& projection = gVR->GetProjection(gVRRenderTarget.value(), Projection::Stage);
        fixedfunction::gCurrentProjectionMatrix = D3DFromM4(projection);
        fixedfunction::gCurrentProjectionMatrixInverse = glm::inverse(projection);
        return hooks::settransform.call(This, State, &fixedfunction::gCurrentProjectionMatrix);
    } else if (gVRRenderTarget && State == D3DTS_VIEW) {
        fixedfunction::gCurrentViewMatrix = D3DFromM4(gVR->GetEyePos(gVRRenderTarget.value()) * gVR->GetPose() * gFlipZMatrix * gVR->GetHorizonLock() * M4FromD3D(*pMatrix));
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

uint32_t __stdcall RBRHook_LoadTexture(void* p, const char* texName, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t g, uint32_t h, uint32_t i, uint32_t j, uint32_t k, IDirect3DTexture9** ppTexture)
{
    auto ret = hooks::loadtexture.call(p, texName, a, b, c, d, e, f, g, h, i, j, k, ppTexture);
    auto tex = std::string(texName)
        | std::ranges::views::transform([](char c) { return std::tolower(c); })
        | std::ranges::to<std::string>();

    if (ret == 0 && (tex.starts_with("cars\\") || tex.starts_with("cars/"))) {
        gCarTextures[tex] = *ppTexture;
    }
    return ret;
}

HRESULT __stdcall DXHook_DrawIndexedPrimitive(IDirect3DDevice9* This, D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{
    IDirect3DVertexShader9* shader;
    IDirect3DBaseTexture9* texture;
    This->GetVertexShader(&shader);
    This->GetTexture(0, &texture);
    if (gVRRenderTarget && !shader && !texture) {
        // Some parts of the car are drawn without a shader and without a texture (just black meshes)
        // We need to render these with the cockpit Z clipping values in order for them to look correct
        const auto projection = fixedfunction::gCurrentProjectionMatrix;
        const auto cockpitProjection = D3DFromM4(gVR->GetProjection(gVRRenderTarget.value(), Projection::Cockpit));
        hooks::settransform.call(This, D3DTS_PROJECTION, &cockpitProjection);
        auto ret = hooks::drawindexedprimitive.call(This, PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
        hooks::settransform.call(This, D3DTS_PROJECTION, &fixedfunction::gCurrentProjectionMatrix);
        return ret;
    } else {
        if (shader)
            shader->Release();
        if (texture)
            texture->Release();
        return hooks::drawindexedprimitive.call(This, PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
    }
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
    if (gCfg.msaa != D3DMULTISAMPLE_NONE) {
        DWORD q = 0;
        if (This->CheckDeviceMultiSampleType(Adapter, DeviceType, D3DFMT_X8R8G8B8, true, gCfg.msaa, &q) != D3D_OK) {
            Dbg("Selected MSAA mode not supported");
            gCfg.msaa = D3DMULTISAMPLE_NONE;
        }
        pPresentationParameters->MultiSampleType = gCfg.msaa;
        pPresentationParameters->MultiSampleQuality = q > 0 ? q - 1 : 0;
    }
    auto ret = hooks::createdevice.call(This, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, &dev);
    if (FAILED(ret)) {
        Dbg("D3D initialization failed: CreateDevice");
        return ret;
    }

    D3DCAPS9 caps;
    dev->GetDeviceCaps(&caps);

    if (static_cast<int32_t>(caps.MaxAnisotropy) < gCfg.anisotropy) {
        gCfg.anisotropy = caps.MaxAnisotropy;
    }

    *ppReturnedDeviceInterface = dev;
    if (gCfg.msaa != D3DMULTISAMPLE_NONE) {
        dev->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, 1);
    }

    for (auto i = D3DVERTEXTEXTURESAMPLER0; i <= D3DVERTEXTEXTURESAMPLER3; ++i) {
        dev->SetSamplerState(i, D3DSAMP_MAXANISOTROPY, gCfg.anisotropy);
    }

    auto devvtbl = GetVtable<IDirect3DDevice9Vtbl>(dev);
    try {
        hooks::setvertexshaderconstantf = Hook(devvtbl->SetVertexShaderConstantF, DXHook_SetVertexShaderConstantF);
        hooks::settransform = Hook(devvtbl->SetTransform, DXHook_SetTransform);
        hooks::present = Hook(devvtbl->Present, DXHook_Present);
        hooks::createvertexshader = Hook(devvtbl->CreateVertexShader, DXHook_CreateVertexShader);
        hooks::drawindexedprimitive = Hook(devvtbl->DrawIndexedPrimitive, DXHook_DrawIndexedPrimitive);
    } catch (const std::runtime_error& e) {
        Dbg(e.what());
    }

    gD3Ddev = dev;

    RECT winBounds;
    GetWindowRect(hFocusWindow, &winBounds);

    if (gVR && gVR->GetRuntimeType() == OPENXR) {
        reinterpret_cast<OpenXR*>(gVR.get())->Init(dev, gCfg, &gD3DVR, winBounds.right, winBounds.bottom);
    } else {
        gVR = std::make_unique<OpenVR>(dev, gCfg, &gD3DVR, winBounds.right, winBounds.bottom);
    }

    // Initialize this pointer here, as it's too early to do this in openRBRVR constructor
    auto rxHandle = GetModuleHandle("Plugins\\rbr_rx.dll");
    if (rxHandle) {
        auto rxAddr = reinterpret_cast<uintptr_t>(rxHandle);
        gBTBTrackStatus = reinterpret_cast<uint8_t*>(rxAddr + RBRRXTrackStatusOffset);

        IDirect3DDevice9Vtbl* rbrrxdev = reinterpret_cast<IDirect3DDevice9Vtbl*>(rxAddr + RBRRXDeviceVtblOffset);
        try {
            hooks::btbsetrendertarget = Hook(rbrrxdev->SetRenderTarget, BTB_SetRenderTarget);
        } catch (const std::runtime_error& e) {
            Dbg(e.what());
        }
    }

    return ret;
}

IDirect3D9* __stdcall DXHook_Direct3DCreate9(UINT SDKVersion)
{
    gVR = std::make_unique<OpenXR>();
    auto d3d = hooks::create.call(SDKVersion);
    if (!d3d) {
        Dbg("Could not initialize Vulkan");
        return nullptr;
    }
    auto d3dvtbl = GetVtable<IDirect3D9Vtbl>(d3d);
    try {
        hooks::createdevice = Hook(d3dvtbl->CreateDevice, DXHook_CreateDevice);
    } catch (const std::runtime_error& e) {
        Dbg(e.what());
    }
    return d3d;
}

openRBRVR::openRBRVR(IRBRGame* g)
    : game(g)
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

    try {
        hooks::create = Hook(d3dcreate, DXHook_Direct3DCreate9);
        hooks::render = Hook(*reinterpret_cast<decltype(RBRHook_Render)*>(RBRRenderFunctionAddr), RBRHook_Render);
    } catch (const std::runtime_error& e) {
        Dbg(e.what());
    }
    gCfg = gSavedCfg = Config::fromFile("Plugins\\openRBRVR.ini");
}

openRBRVR::~openRBRVR()
{
    if (gVR) {
        gVR->ShutdownVR();
    }
}

void openRBRVR::HandleFrontEndEvents(char txtKeyboard, bool bUp, bool bDown, bool bLeft, bool bRight, bool bSelect)
{
    if (bSelect) {
        gMenu->Select();
    }
    if (bUp) {
        gMenu->Up();
    }
    if (bDown) {
        gMenu->Down();
    }
    if (bLeft) {
        gMenu->Left();
    }
    if (bRight) {
        gMenu->Right();
    }
}

void openRBRVR::DrawMenuEntries(const std::ranges::forward_range auto& entries, float rowHeight)
{
    auto x = 0.0f;
    auto y = 0.0f;
    for (const auto& entry : entries) {
        if (entry.font) {
            game->SetFont(entry.font.value());
        }
        if (entry.menuColor) {
            game->SetMenuColor(entry.menuColor.value());
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
    const auto idx = gMenu->Index();
    const auto isLicenseMenu = idx < 0;

    game->DrawBlackOut(0.0f, isLicenseMenu ? 88.0f : 221.0f, 800.0f, 10.0f);
    if (!isLicenseMenu) {
        game->DrawSelection(0.0f, menuItemsStartHeight - 2.0f + (static_cast<float>(gMenu->Index()) * gMenu->RowHeight()), 440.0f);
    }

    game->SetFont(IRBRGame::EFonts::FONT_BIG);
    game->SetMenuColor(IRBRGame::EMenuColors::MENU_HEADING);
    game->WriteText(65.0f, 49.0f, gMenu->Heading());

    const auto& entries = gMenu->Entries();
    DrawMenuEntries(entries, gMenu->RowHeight());

    game->SetFont(IRBRGame::EFonts::FONT_BIG);
    game->SetMenuColor(IRBRGame::EMenuColors::MENU_TEXT);

    auto i = 0;
    if (!isLicenseMenu) {
        for (const auto& txt : entries[gMenu->Index()].longText) {
            game->WriteText(65.0f, 221.0f + ((i + 1) * gMenu->RowHeight()), txt.c_str());
            i++;
        }
    }
}
