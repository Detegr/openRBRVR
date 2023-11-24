#pragma once

#include <d3d9.h>

#define VK_USE_PLATFORM_WIN32_KHR 1
#include "vulkan/vulkan.h"
#undef VK_USE_PLATFORM_WIN32_KHR

struct D3D9_TEXTURE_VR_DESC
{
  uint64_t Image;
  VkDevice Device;
  VkPhysicalDevice PhysicalDevice;
  VkInstance Instance;
  VkQueue Queue;
  uint32_t QueueFamilyIndex;

  uint32_t Width;
  uint32_t Height;
  VkFormat Format;
  uint32_t SampleCount;
};

struct OXR_VK_DEVICE_DESC
{
  VkDevice Device;
  VkPhysicalDevice PhysicalDevice;
  VkInstance Instance;
  uint32_t QueueIndex;
  uint32_t QueueFamilyIndex;
};

// Remember: this class is very similar to D3D9VkInteropDevice introduced later.
//           Keep an eye on that class for sync changes.
//
// Remember: add new functions at the end to improve back compat.
MIDL_INTERFACE("7e272b32-a49c-46c7-b1a4-ef52936bec87")
IDirect3DVR9 : public IUnknown
{
  virtual HRESULT STDMETHODCALLTYPE GetVRDesc(IDirect3DSurface9 * pSurface,
                                              D3D9_TEXTURE_VR_DESC * pDesc) = 0;
  virtual HRESULT STDMETHODCALLTYPE TransferSurfaceForVR(IDirect3DSurface9 *
                                                         pSurface) = 0;
  virtual HRESULT STDMETHODCALLTYPE BeginVRSubmit() = 0;
  virtual HRESULT STDMETHODCALLTYPE EndVRSubmit() = 0;
  virtual HRESULT STDMETHODCALLTYPE LockDevice() = 0;
  virtual HRESULT STDMETHODCALLTYPE UnlockDevice() = 0;

  // Wait* APIs are experimental.
  virtual HRESULT STDMETHODCALLTYPE WaitDeviceIdle(BOOL flush) = 0;
  virtual HRESULT STDMETHODCALLTYPE WaitGraphicsQueueIdle(BOOL flush) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetOXRVkDeviceDesc(OXR_VK_DEVICE_DESC*
                                                       vkDeviceDescOut) = 0;
};

#ifdef _MSC_VER
struct __declspec(uuid("7e272b32-a49c-46c7-b1a4-ef52936bec87")) IDirect3DVR9;
#else
__CRT_UUID_DECL(IDirect3DVR9,
                0x7e272b32,
                0xa49c,
                0x46c7,
                0xb1,
                0xa4,
                0xef,
                0x52,
                0x93,
                0x6b,
                0xec,
                0x87);
#endif

HRESULT __stdcall Direct3DCreateVRImpl(IDirect3DDevice9* pDevice,
                                       IDirect3DVR9** pInterface);
