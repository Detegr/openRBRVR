// Optional real D3D9 smoke test. Creates a hidden window, renders both embedded
// glove assets with the plugin's skinning/texture upload code, then saves a BMP.
#include "HandMesh.hpp"
#include "HandTexture.hpp"
#include <ext/matrix_clip_space.hpp>
#include <ext/matrix_transform.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
    void check(HRESULT result, const char* operation)
    {
        if (FAILED(result))
            throw std::runtime_error(std::format("{} failed: 0x{:08x}", operation, static_cast<uint32_t>(result)));
    }

    HandTracking::Hand pose(XrHandEXT side, bool curl, const M4& world)
    {
        const auto& model = hand_model(side);
        std::array<M4, XR_HAND_JOINT_COUNT_EXT> rest, current;
        for (size_t i = 0; i < rest.size(); ++i)
            rest[i] = glm::inverse(glm::make_mat4(model.inverse_bind[i].data()));
        current[XR_HAND_JOINT_WRIST_EXT] = rest[XR_HAND_JOINT_WRIST_EXT];
        current[XR_HAND_JOINT_PALM_EXT] = rest[XR_HAND_JOINT_PALM_EXT];
        for (int i = XR_HAND_JOINT_THUMB_METACARPAL_EXT; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
            const bool metacarpal = i == 2 || i == 6 || i == 11 || i == 16 || i == 21;
            const int parent = metacarpal ? XR_HAND_JOINT_WRIST_EXT : i - 1;
            auto local = glm::inverse(rest[parent]) * rest[i];
            if (curl && !metacarpal) {
                const int bone = i < 6 ? i - 2 : (i - 6) % 5;
                const float bend = bone == 1 ? 0.8f : bone == 2 ? 0.9f
                    : bone == 3                                 ? 0.5f
                                                                : 0.0f;
                local *= glm::rotate(M4(1), -bend, glm::vec3(1, 0, 0));
            }
            current[i] = current[parent] * local;
        }
        // Place the model by its palm, independent of the source scene's axes.
        const auto placement = world * glm::inverse(rest[XR_HAND_JOINT_PALM_EXT]);
        HandTracking::Hand hand;
        hand.active = true;
        for (size_t i = 0; i < hand.joints.size(); ++i) {
            const auto at = placement * current[i];
            const auto q = glm::normalize(glm::quat_cast(at));
            hand.joints[i].pose = { { q.x, q.y, q.z, q.w }, { at[3].x, at[3].y, at[3].z } };
        }
        return hand;
    }
}

int main(int argc, char** argv)
{
    try {
        const char* output = argc > 1 ? argv[1] : "zig-out/valve-gloves.bmp";
        const auto window = CreateWindowExA(0, "STATIC", "Valve glove preview", WS_OVERLAPPEDWINDOW,
            0, 0, 1200, 800, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
        if (!window)
            throw std::runtime_error("Could not create hidden preview window");
        auto* d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d)
            throw std::runtime_error("Direct3DCreate9 failed");
        D3DPRESENT_PARAMETERS parameters {};
        parameters.BackBufferWidth = 1200;
        parameters.BackBufferHeight = 800;
        parameters.BackBufferFormat = D3DFMT_X8R8G8B8;
        parameters.BackBufferCount = 1;
        parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
        parameters.Windowed = TRUE;
        parameters.hDeviceWindow = window;
        parameters.EnableAutoDepthStencil = TRUE;
        parameters.AutoDepthStencilFormat = D3DFMT_D24S8;
        IDirect3DDevice9* dev = nullptr;
        check(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                  D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &parameters, &dev),
            "CreateDevice");
        const auto& texture_data = hand_texture_data();
        IDirect3DTexture9* texture = nullptr;
        check(dev->CreateTexture(512, 512, static_cast<UINT>(texture_data.levels.size()), 0,
                  D3DFMT_DXT1, D3DPOOL_MANAGED, &texture, nullptr),
            "CreateTexture");
        check(upload_hand_texture(texture, texture_data), "UploadTexture");

        HandMesh mesh;
        for (int row = 0; row < 2; ++row) {
            const auto side = row == 0 ? XR_HAND_LEFT_EXT : XR_HAND_RIGHT_EXT;
            for (int column = 0; column < 3; ++column) {
                auto world = glm::translate(M4(1), glm::vec3((column - 1) * 0.21f, row == 0 ? 0.095f : -0.105f, 0));
                world *= glm::rotate(M4(1), glm::radians(column == 1 ? -90.0f : 90.0f), glm::vec3(1, 0, 0));
                const auto hand = pose(side, column == 2, world);
                append_hand_mesh(hand, side, mesh);
            }
        }
        check(dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(44, 49, 58), 1.0f, 0), "Clear");
        check(dev->BeginScene(), "BeginScene");
        check(dev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1), "SetFVF");
        const auto identity = d3d_from_m4(M4(1));
        const auto view = d3d_from_m4(glm::lookAtRH(glm::vec3(0, 0, 0.6f), glm::vec3(0), glm::vec3(0, 1, 0)));
        const auto projection = d3d_from_m4(glm::orthoRH_ZO(-0.32f, 0.32f, -0.215f, 0.215f, 0.01f, 2.0f));
        check(dev->SetTransform(D3DTS_WORLD, &identity), "World");
        check(dev->SetTransform(D3DTS_VIEW, &view), "View");
        check(dev->SetTransform(D3DTS_PROJECTION, &projection), "Projection");
        check(dev->SetRenderState(D3DRS_LIGHTING, FALSE), "Lighting");
        check(dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE), "Culling");
        check(dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE), "Depth");
        check(dev->SetTexture(0, texture), "SetTexture");
        check(dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE), "ColorOp");
        check(dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE), "ColorTexture");
        check(dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE), "ColorDiffuse");
        check(dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR), "MinFilter");
        check(dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR), "MagFilter");
        check(dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR), "MipFilter");
        check(dev->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(mesh.vertices.size()),
                  static_cast<UINT>(mesh.indices.size() / 3), mesh.indices.data(), D3DFMT_INDEX16, mesh.vertices.data(), sizeof(HandVertex)),
            "DrawIndexedPrimitiveUP");
        check(dev->EndScene(), "EndScene");

        IDirect3DSurface9* target = nullptr;
        IDirect3DSurface9* copy = nullptr;
        check(dev->GetRenderTarget(0, &target), "GetRenderTarget");
        check(dev->CreateOffscreenPlainSurface(1200, 800, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &copy, nullptr), "CreateReadback");
        check(dev->GetRenderTargetData(target, copy), "Readback");
        D3DLOCKED_RECT lock {};
        check(copy->LockRect(&lock, nullptr, D3DLOCK_READONLY), "LockReadback");
        BITMAPFILEHEADER header {};
        header.bfType = 0x4d42;
        header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        header.bfSize = header.bfOffBits + 1200 * 800 * 4;
        BITMAPINFOHEADER info {};
        info.biSize = sizeof(info);
        info.biWidth = 1200;
        info.biHeight = -800;
        info.biPlanes = 1;
        info.biBitCount = 32;
        std::ofstream file(output, std::ios::binary);
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(&info), sizeof(info));
        for (int row = 0; row < 800; ++row)
            file.write(static_cast<const char*>(lock.pBits) + row * lock.Pitch, 1200 * 4);
        if (!file)
            throw std::runtime_error("Could not save preview BMP");
        check(copy->UnlockRect(), "UnlockReadback");
        copy->Release();
        target->Release();
        texture->Release();
        dev->Release();
        d3d->Release();
        DestroyWindow(window);
        std::cout << "D3D9 glove smoke test passed; saved " << output << " (left above right; back, palm, curled).\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
