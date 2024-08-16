#pragma once

#include <d3d9.h>
#include <d3d9_vr.h>

extern "C" HRESULT __stdcall Direct3DCreateVR(IDirect3DDevice9* pDevice, IDirect3DVR9** pInterface);

// The vtable definitions are from Mesa(https://www.mesa3d.org) under following license:
/*
 * Copyright 2011 Joakim Sindholt <opensource@zhasha.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE. */

// clang-format off

typedef struct IDirect3D9Vtbl {
    /* IUnknown */
    HRESULT(WINAPI* QueryInterface)(IDirect3D9* This, REFIID riid, void** ppvObject);
    ULONG(WINAPI* AddRef)(IDirect3D9* This);
    ULONG(WINAPI* Release)(IDirect3D9* This);
    /* IDirect3D9 */
    HRESULT(WINAPI* RegisterSoftwareDevice)(IDirect3D9* This, void* pInitializeFunction);
    UINT(WINAPI* GetAdapterCount)(IDirect3D9* This);
    HRESULT(WINAPI* GetAdapterIdentifier)(IDirect3D9* This, UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier);
    UINT(WINAPI* GetAdapterModeCount)(IDirect3D9* This, UINT Adapter, D3DFORMAT Format);
    HRESULT(WINAPI* EnumAdapterModes)(IDirect3D9* This, UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode);
    HRESULT(WINAPI* GetAdapterDisplayMode)(IDirect3D9* This, UINT Adapter, D3DDISPLAYMODE* pMode);
    HRESULT(WINAPI* CheckDeviceType)(IDirect3D9* This, UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed);
    HRESULT(WINAPI* CheckDeviceFormat)(IDirect3D9* This, UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat);
    HRESULT(WINAPI* CheckDeviceMultiSampleType)(IDirect3D9* This, UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels);
    HRESULT(WINAPI* CheckDepthStencilMatch)(IDirect3D9* This, UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat);
    HRESULT(WINAPI* CheckDeviceFormatConversion)(IDirect3D9* This, UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat);
    HRESULT(WINAPI* GetDeviceCaps)(IDirect3D9* This, UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps);
    HMONITOR(WINAPI* GetAdapterMonitor)(IDirect3D9* This, UINT Adapter);
    HRESULT(WINAPI* CreateDevice)(IDirect3D9* This, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface);
} IDirect3D9Vtbl;

typedef struct IDirect3DDevice9Vtbl
{
	/* IUnknown */
	HRESULT (WINAPI *QueryInterface)(IDirect3DDevice9 *This, REFIID riid, void **ppvObject);
	ULONG (WINAPI *AddRef)(IDirect3DDevice9 *This);
	ULONG (WINAPI *Release)(IDirect3DDevice9 *This);
	/* IDirect3DDevice9 */
	HRESULT (WINAPI *TestCooperativeLevel)(IDirect3DDevice9 *This);
	UINT (WINAPI *GetAvailableTextureMem)(IDirect3DDevice9 *This);
	HRESULT (WINAPI *EvictManagedResources)(IDirect3DDevice9 *This);
	HRESULT (WINAPI *GetDirect3D)(IDirect3DDevice9 *This, IDirect3D9 **ppD3D9);
	HRESULT (WINAPI *GetDeviceCaps)(IDirect3DDevice9 *This, D3DCAPS9 *pCaps);
	HRESULT (WINAPI *GetDisplayMode)(IDirect3DDevice9 *This, UINT iSwapChain, D3DDISPLAYMODE *pMode);
	HRESULT (WINAPI *GetCreationParameters)(IDirect3DDevice9 *This, D3DDEVICE_CREATION_PARAMETERS *pParameters);
	HRESULT (WINAPI *SetCursorProperties)(IDirect3DDevice9 *This, UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9 *pCursorBitmap);
	void (WINAPI *SetCursorPosition)(IDirect3DDevice9 *This, int X, int Y, DWORD Flags);
	BOOL (WINAPI *ShowCursor)(IDirect3DDevice9 *This, BOOL bShow);
	HRESULT (WINAPI *CreateAdditionalSwapChain)(IDirect3DDevice9 *This, D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DSwapChain9 **pSwapChain);
	HRESULT (WINAPI *GetSwapChain)(IDirect3DDevice9 *This, UINT iSwapChain, IDirect3DSwapChain9 **pSwapChain);
	UINT (WINAPI *GetNumberOfSwapChains)(IDirect3DDevice9 *This);
	HRESULT (WINAPI *Reset)(IDirect3DDevice9 *This, D3DPRESENT_PARAMETERS *pPresentationParameters);
	HRESULT (WINAPI *Present)(IDirect3DDevice9 *This, const RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion);
	HRESULT (WINAPI *GetBackBuffer)(IDirect3DDevice9 *This, UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer);
	HRESULT (WINAPI *GetRasterStatus)(IDirect3DDevice9 *This, UINT iSwapChain, D3DRASTER_STATUS *pRasterStatus);
	HRESULT (WINAPI *SetDialogBoxMode)(IDirect3DDevice9 *This, BOOL bEnableDialogs);
	void (WINAPI *SetGammaRamp)(IDirect3DDevice9 *This, UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP *pRamp);
	void (WINAPI *GetGammaRamp)(IDirect3DDevice9 *This, UINT iSwapChain, D3DGAMMARAMP *pRamp);
	HRESULT (WINAPI *CreateTexture)(IDirect3DDevice9 *This, UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 **ppTexture, HANDLE *pSharedHandle);
	HRESULT (WINAPI *CreateVolumeTexture)(IDirect3DDevice9 *This, UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9 **ppVolumeTexture, HANDLE *pSharedHandle);
	HRESULT (WINAPI *CreateCubeTexture)(IDirect3DDevice9 *This, UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9 **ppCubeTexture, HANDLE *pSharedHandle);
	HRESULT (WINAPI *create_vertex_buffer)(IDirect3DDevice9 *This, UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle);
	HRESULT (WINAPI *CreateIndexBuffer)(IDirect3DDevice9 *This, UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9 **ppIndexBuffer, HANDLE *pSharedHandle);
	HRESULT (WINAPI *CreateRenderTarget)(IDirect3DDevice9 *This, UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle);
	HRESULT (WINAPI *CreateDepthStencilSurface)(IDirect3DDevice9 *This, UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle);
	HRESULT (WINAPI *UpdateSurface)(IDirect3DDevice9 *This, IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestinationSurface, const POINT *pDestPoint);
	HRESULT (WINAPI *UpdateTexture)(IDirect3DDevice9 *This, IDirect3DBaseTexture9 *pSourceTexture, IDirect3DBaseTexture9 *pDestinationTexture);
	HRESULT (WINAPI *GetRenderTargetData)(IDirect3DDevice9 *This, IDirect3DSurface9 *pRenderTarget, IDirect3DSurface9 *pDestSurface);
	HRESULT (WINAPI *GetFrontBufferData)(IDirect3DDevice9 *This, UINT iSwapChain, IDirect3DSurface9 *pDestSurface);
	HRESULT (WINAPI *StretchRect)(IDirect3DDevice9 *This, IDirect3DSurface9 *pSourceSurface, const RECT *pSourceRect, IDirect3DSurface9 *pDestSurface, const RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter);
	HRESULT (WINAPI *ColorFill)(IDirect3DDevice9 *This, IDirect3DSurface9 *pSurface, const RECT *pRect, D3DCOLOR color);
	HRESULT (WINAPI *CreateOffscreenPlainSurface)(IDirect3DDevice9 *This, UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle);
	HRESULT (WINAPI *SetRenderTarget)(IDirect3DDevice9 *This, DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget);
	HRESULT (WINAPI *GetRenderTarget)(IDirect3DDevice9 *This, DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget);
	HRESULT (WINAPI *SetDepthStencilSurface)(IDirect3DDevice9 *This, IDirect3DSurface9 *pNewZStencil);
	HRESULT (WINAPI *GetDepthStencilSurface)(IDirect3DDevice9 *This, IDirect3DSurface9 **ppZStencilSurface);
	HRESULT (WINAPI *BeginScene)(IDirect3DDevice9 *This);
	HRESULT (WINAPI *EndScene)(IDirect3DDevice9 *This);
	HRESULT (WINAPI *Clear)(IDirect3DDevice9 *This, DWORD Count, const D3DRECT *pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil);
	HRESULT (WINAPI *SetTransform)(IDirect3DDevice9 *This, D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix);
	HRESULT (WINAPI *GetTransform)(IDirect3DDevice9 *This, D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix);
	HRESULT (WINAPI *MultiplyTransform)(IDirect3DDevice9 *This, D3DTRANSFORMSTATETYPE State, const D3DMATRIX *pMatrix);
	HRESULT (WINAPI *SetViewport)(IDirect3DDevice9 *This, const D3DVIEWPORT9 *pViewport);
	HRESULT (WINAPI *GetViewport)(IDirect3DDevice9 *This, D3DVIEWPORT9 *pViewport);
	HRESULT (WINAPI *SetMaterial)(IDirect3DDevice9 *This, const D3DMATERIAL9 *pMaterial);
	HRESULT (WINAPI *GetMaterial)(IDirect3DDevice9 *This, D3DMATERIAL9 *pMaterial);
	HRESULT (WINAPI *SetLight)(IDirect3DDevice9 *This, DWORD Index, const D3DLIGHT9 *pLight);
	HRESULT (WINAPI *GetLight)(IDirect3DDevice9 *This, DWORD Index, D3DLIGHT9 *pLight);
	HRESULT (WINAPI *LightEnable)(IDirect3DDevice9 *This, DWORD Index, BOOL Enable);
	HRESULT (WINAPI *GetLightEnable)(IDirect3DDevice9 *This, DWORD Index, BOOL *pEnable);
	HRESULT (WINAPI *SetClipPlane)(IDirect3DDevice9 *This, DWORD Index, const float *pPlane);
	HRESULT (WINAPI *GetClipPlane)(IDirect3DDevice9 *This, DWORD Index, float *pPlane);
	HRESULT (WINAPI *SetRenderState)(IDirect3DDevice9 *This, D3DRENDERSTATETYPE State, DWORD Value);
	HRESULT (WINAPI *GetRenderState)(IDirect3DDevice9 *This, D3DRENDERSTATETYPE State, DWORD *pValue);
	HRESULT (WINAPI *CreateStateBlock)(IDirect3DDevice9 *This, D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9 **ppSB);
	HRESULT (WINAPI *BeginStateBlock)(IDirect3DDevice9 *This);
	HRESULT (WINAPI *EndStateBlock)(IDirect3DDevice9 *This, IDirect3DStateBlock9 **ppSB);
	HRESULT (WINAPI *SetClipStatus)(IDirect3DDevice9 *This, const D3DCLIPSTATUS9 *pClipStatus);
	HRESULT (WINAPI *GetClipStatus)(IDirect3DDevice9 *This, D3DCLIPSTATUS9 *pClipStatus);
	HRESULT (WINAPI *GetTexture)(IDirect3DDevice9 *This, DWORD Stage, IDirect3DBaseTexture9 **ppTexture);
	HRESULT (WINAPI *SetTexture)(IDirect3DDevice9 *This, DWORD Stage, IDirect3DBaseTexture9 *pTexture);
	HRESULT (WINAPI *GetTextureStageState)(IDirect3DDevice9 *This, DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD *pValue);
	HRESULT (WINAPI *SetTextureStageState)(IDirect3DDevice9 *This, DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
	HRESULT (WINAPI *GetSamplerState)(IDirect3DDevice9 *This, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD *pValue);
	HRESULT (WINAPI *SetSamplerState)(IDirect3DDevice9 *This, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value);
	HRESULT (WINAPI *ValidateDevice)(IDirect3DDevice9 *This, DWORD *pNumPasses);
	HRESULT (WINAPI *SetPaletteEntries)(IDirect3DDevice9 *This, UINT PaletteNumber, const PALETTEENTRY *pEntries);
	HRESULT (WINAPI *GetPaletteEntries)(IDirect3DDevice9 *This, UINT PaletteNumber, PALETTEENTRY *pEntries);
	HRESULT (WINAPI *SetCurrentTexturePalette)(IDirect3DDevice9 *This, UINT PaletteNumber);
	HRESULT (WINAPI *GetCurrentTexturePalette)(IDirect3DDevice9 *This, UINT *PaletteNumber);
	HRESULT (WINAPI *SetScissorRect)(IDirect3DDevice9 *This, const RECT *pRect);
	HRESULT (WINAPI *GetScissorRect)(IDirect3DDevice9 *This, RECT *pRect);
	HRESULT (WINAPI *SetSoftwareVertexProcessing)(IDirect3DDevice9 *This, BOOL bSoftware);
	BOOL (WINAPI *GetSoftwareVertexProcessing)(IDirect3DDevice9 *This);
	HRESULT (WINAPI *SetNPatchMode)(IDirect3DDevice9 *This, float nSegments);
	float (WINAPI *GetNPatchMode)(IDirect3DDevice9 *This);
	HRESULT (WINAPI *DrawPrimitive)(IDirect3DDevice9 *This, D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount);
	HRESULT (WINAPI *DrawIndexedPrimitive)(IDirect3DDevice9 *This, D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount);
	HRESULT (WINAPI *DrawPrimitiveUP)(IDirect3DDevice9 *This, D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride);
	HRESULT (WINAPI *DrawIndexedPrimitiveUP)(IDirect3DDevice9 *This, D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, const void *pIndexData, D3DFORMAT IndexDataFormat, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride);
	HRESULT (WINAPI *ProcessVertices)(IDirect3DDevice9 *This, UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9 *pDestBuffer, IDirect3DVertexDeclaration9 *pVertexDecl, DWORD Flags);
	HRESULT (WINAPI *CreateVertexDeclaration)(IDirect3DDevice9 *This, const D3DVERTEXELEMENT9 *pVertexElements, IDirect3DVertexDeclaration9 **ppDecl);
	HRESULT (WINAPI *SetVertexDeclaration)(IDirect3DDevice9 *This, IDirect3DVertexDeclaration9 *pDecl);
	HRESULT (WINAPI *GetVertexDeclaration)(IDirect3DDevice9 *This, IDirect3DVertexDeclaration9 **ppDecl);
	HRESULT (WINAPI *SetFVF)(IDirect3DDevice9 *This, DWORD FVF);
	HRESULT (WINAPI *GetFVF)(IDirect3DDevice9 *This, DWORD *pFVF);
	HRESULT (WINAPI *CreateVertexShader)(IDirect3DDevice9 *This, const DWORD *pFunction, IDirect3DVertexShader9 **ppShader);
	HRESULT (WINAPI *SetVertexShader)(IDirect3DDevice9 *This, IDirect3DVertexShader9 *pShader);
	HRESULT (WINAPI *GetVertexShader)(IDirect3DDevice9 *This, IDirect3DVertexShader9 **ppShader);
	HRESULT (WINAPI *SetVertexShaderConstantF)(IDirect3DDevice9 *This, UINT StartRegister, const float *pConstantData, UINT Vector4fCount);
	HRESULT (WINAPI *GetVertexShaderConstantF)(IDirect3DDevice9 *This, UINT StartRegister, float *pConstantData, UINT Vector4fCount);
	HRESULT (WINAPI *SetVertexShaderConstantI)(IDirect3DDevice9 *This, UINT StartRegister, const int *pConstantData, UINT Vector4iCount);
	HRESULT (WINAPI *GetVertexShaderConstantI)(IDirect3DDevice9 *This, UINT StartRegister, int *pConstantData, UINT Vector4iCount);
	HRESULT (WINAPI *SetVertexShaderConstantB)(IDirect3DDevice9 *This, UINT StartRegister, const BOOL *pConstantData, UINT BoolCount);
	HRESULT (WINAPI *GetVertexShaderConstantB)(IDirect3DDevice9 *This, UINT StartRegister, BOOL *pConstantData, UINT BoolCount);
	HRESULT (WINAPI *SetStreamSource)(IDirect3DDevice9 *This, UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride);
	HRESULT (WINAPI *GetStreamSource)(IDirect3DDevice9 *This, UINT StreamNumber, IDirect3DVertexBuffer9 **ppStreamData, UINT *pOffsetInBytes, UINT *pStride);
	HRESULT (WINAPI *SetStreamSourceFreq)(IDirect3DDevice9 *This, UINT StreamNumber, UINT Setting);
	HRESULT (WINAPI *GetStreamSourceFreq)(IDirect3DDevice9 *This, UINT StreamNumber, UINT *pSetting);
	HRESULT (WINAPI *SetIndices)(IDirect3DDevice9 *This, IDirect3DIndexBuffer9 *pIndexData);
	HRESULT (WINAPI *GetIndices)(IDirect3DDevice9 *This, IDirect3DIndexBuffer9 **ppIndexData, UINT *pBaseVertexIndex);
	HRESULT (WINAPI *CreatePixelShader)(IDirect3DDevice9 *This, const DWORD *pFunction, IDirect3DPixelShader9 **ppShader);
	HRESULT (WINAPI *SetPixelShader)(IDirect3DDevice9 *This, IDirect3DPixelShader9 *pShader);
	HRESULT (WINAPI *GetPixelShader)(IDirect3DDevice9 *This, IDirect3DPixelShader9 **ppShader);
	HRESULT (WINAPI *SetPixelShaderConstantF)(IDirect3DDevice9 *This, UINT StartRegister, const float *pConstantData, UINT Vector4fCount);
	HRESULT (WINAPI *GetPixelShaderConstantF)(IDirect3DDevice9 *This, UINT StartRegister, float *pConstantData, UINT Vector4fCount);
	HRESULT (WINAPI *SetPixelShaderConstantI)(IDirect3DDevice9 *This, UINT StartRegister, const int *pConstantData, UINT Vector4iCount);
	HRESULT (WINAPI *GetPixelShaderConstantI)(IDirect3DDevice9 *This, UINT StartRegister, int *pConstantData, UINT Vector4iCount);
	HRESULT (WINAPI *SetPixelShaderConstantB)(IDirect3DDevice9 *This, UINT StartRegister, const BOOL *pConstantData, UINT BoolCount);
	HRESULT (WINAPI *GetPixelShaderConstantB)(IDirect3DDevice9 *This, UINT StartRegister, BOOL *pConstantData, UINT BoolCount);
	HRESULT (WINAPI *DrawRectPatch)(IDirect3DDevice9 *This, UINT Handle, const float *pNumSegs, const D3DRECTPATCH_INFO *pRectPatchInfo);
	HRESULT (WINAPI *DrawTriPatch)(IDirect3DDevice9 *This, UINT Handle, const float *pNumSegs, const D3DTRIPATCH_INFO *pTriPatchInfo);
	HRESULT (WINAPI *DeletePatch)(IDirect3DDevice9 *This, UINT Handle);
	HRESULT (WINAPI *CreateQuery)(IDirect3DDevice9 *This, D3DQUERYTYPE Type, IDirect3DQuery9 **ppQuery);
} IDirect3DDevice9Vtbl;

typedef struct IDirect3DStateBlock9Vtbl
{
	/* IUnknown */
	HRESULT (WINAPI *QueryInterface)(IDirect3DStateBlock9 *This, REFIID riid, void **ppvObject);
	ULONG (WINAPI *AddRef)(IDirect3DStateBlock9 *This);
	ULONG (WINAPI *Release)(IDirect3DStateBlock9 *This);
	/* IDirect3DStateBlock9 */
	HRESULT (WINAPI *GetDevice)(IDirect3DStateBlock9 *This, IDirect3DDevice9 **ppDevice);
	HRESULT (WINAPI *Capture)(IDirect3DStateBlock9 *This);
	HRESULT (WINAPI *Apply)(IDirect3DStateBlock9 *This);
} IDirect3DStateBlock9Vtbl;

// clang-format on
