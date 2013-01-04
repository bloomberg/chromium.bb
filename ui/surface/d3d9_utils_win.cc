// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/surface/d3d9_utils_win.h"

#include "base/file_path.h"
#include "base/scoped_native_library.h"
#include "base/win/scoped_comptr.h"
#include "ui/gfx/size.h"

namespace {

const wchar_t kD3D9ModuleName[] = L"d3d9.dll";
const char kCreate3D9DeviceExName[] = "Direct3DCreate9Ex";
typedef HRESULT (WINAPI *Direct3DCreate9ExFunc)(UINT sdk_version,
                                                IDirect3D9Ex **d3d);
}  // namespace

namespace ui_surface_d3d9_utils {

bool LoadD3D9(base::ScopedNativeLibrary* storage) {
  storage->Reset(base::LoadNativeLibrary(FilePath(kD3D9ModuleName), NULL));
  return storage->is_valid();
}

bool CreateDevice(const base::ScopedNativeLibrary& d3d_module,
                  D3DDEVTYPE device_type,
                  uint32 presentation_interval,
                  IDirect3DDevice9Ex** device) {

  Direct3DCreate9ExFunc create_func = reinterpret_cast<Direct3DCreate9ExFunc>(
      d3d_module.GetFunctionPointer(kCreate3D9DeviceExName));
  if (!create_func)
    return false;

  base::win::ScopedComPtr<IDirect3D9Ex> d3d;
  HRESULT hr = create_func(D3D_SDK_VERSION, d3d.Receive());
  if (FAILED(hr))
    return false;

  // Any old window will do to create the device. In practice the window to
  // present to is an argument to IDirect3DDevice9::Present.
  HWND window = GetShellWindow();

  D3DPRESENT_PARAMETERS parameters = { 0 };
  parameters.BackBufferWidth = 1;
  parameters.BackBufferHeight = 1;
  parameters.BackBufferCount = 1;
  parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
  parameters.hDeviceWindow = window;
  parameters.Windowed = TRUE;
  parameters.Flags = 0;
  parameters.PresentationInterval = presentation_interval;
  parameters.SwapEffect = D3DSWAPEFFECT_COPY;

  hr = d3d->CreateDeviceEx(
      D3DADAPTER_DEFAULT,
      device_type,
      window,
      D3DCREATE_FPU_PRESERVE | D3DCREATE_SOFTWARE_VERTEXPROCESSING |
          D3DCREATE_DISABLE_PSGP_THREADING | D3DCREATE_MULTITHREADED,
      &parameters,
      NULL,
      device);
  return SUCCEEDED(hr);
}

bool OpenSharedTexture(IDirect3DDevice9* device,
                       int64 surface_handle,
                       const gfx::Size& size,
                       IDirect3DTexture9** opened_texture) {
  HANDLE handle = reinterpret_cast<HANDLE>(surface_handle);
  HRESULT hr = device->CreateTexture(size.width(),
                                     size.height(),
                                     1,
                                     D3DUSAGE_RENDERTARGET,
                                     D3DFMT_A8R8G8B8,
                                     D3DPOOL_DEFAULT,
                                     opened_texture,
                                     &handle);
  return SUCCEEDED(hr);
}

bool CreateTemporaryLockableSurface(IDirect3DDevice9* device,
                                    const gfx::Size& size,
                                    IDirect3DSurface9** surface) {
  HRESULT hr = device->CreateRenderTarget(
        size.width(),
        size.height(),
        D3DFMT_A8R8G8B8,
        D3DMULTISAMPLE_NONE,
        0,
        TRUE,
        surface,
        NULL);
  return SUCCEEDED(hr);
}

bool CreateTemporaryRenderTargetTexture(IDirect3DDevice9* device,
                                        const gfx::Size& size,
                                        IDirect3DTexture9** texture,
                                        IDirect3DSurface9** render_target) {
  HRESULT hr = device->CreateTexture(
        size.width(),
        size.height(),
        1,  // Levels
        D3DUSAGE_RENDERTARGET,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        texture,
        NULL);
  if (!SUCCEEDED(hr))
    return false;
  hr = (*texture)->GetSurfaceLevel(0, render_target);
  return SUCCEEDED(hr);
}

}  // namespace ui_surface_d3d9_utils
