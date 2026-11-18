#pragma once

#include "RenderTarget.hpp"

#include "Globals.hpp"
#include <d3d9.h>

namespace dx {
    inline bool multiview_rendering_enabled() { return g::cfg.multiview && !g::cfg.experimental.disable_multiview; }
    bool add_vertex_shader(IDirect3DVertexShader9* shader);
    void render_vr_eye(void* p, RenderTarget eye, bool clear = true);

    // Hooked functions
    HRESULT __stdcall CreateVertexShader(IDirect3DDevice9* This, const DWORD* pFunction, IDirect3DVertexShader9** ppShader);
    HRESULT __stdcall Present(IDirect3DDevice9* This, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);
    HRESULT __stdcall SetVertexShaderConstantF(IDirect3DDevice9* This, UINT StartRegister, const float* pConstantData, UINT Vector4fCount);
    HRESULT __stdcall SetTransform(IDirect3DDevice9* This, D3DTRANSFORMSTATETYPE State, const D3DMATRIX* pMatrix);
    HRESULT __stdcall DrawIndexedPrimitive(IDirect3DDevice9* This, D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount);
    HRESULT __stdcall CreateDevice(IDirect3D9* This, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface);
    IDirect3D9* __stdcall Direct3DCreate9(UINT SDKVersion);
}
