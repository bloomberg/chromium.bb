// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper functions that Direct3D 9Ex code a little easier to work with for
// the ui/surface code.

#ifndef UI_SURFACE_D3D9_UTILS_WIN_H_
#define UI_SURFACE_D3D9_UTILS_WIN_H_

#include <d3d9.h>

#include "base/basictypes.h"
#include "ui/surface/surface_export.h"

namespace base {
class ScopedNativeLibrary;
}

namespace gfx {
class Size;
}

namespace ui_surface_d3d9_utils {

// Visible for testing. Loads the Direct3D9 library. Returns true on success.
SURFACE_EXPORT
bool LoadD3D9(base::ScopedNativeLibrary* storage);

// Visible for testing. Creates a Direct3D9 device suitable for use with the
// accelerated surface code. Returns true on success.
SURFACE_EXPORT
bool CreateDevice(const base::ScopedNativeLibrary& d3d_module,
                  D3DDEVTYPE device_type,
                  uint32 presentation_interval,
                  IDirect3DDevice9Ex** device);

// Calls the Vista+ (WDDM1.0) variant of CreateTexture that semantically
// opens a texture allocated (possibly in another process) as shared. The
// shared texture is identified by its surface handle. The resulting texture
// is written into |opened_texture|.
//
// Returns true on success.
SURFACE_EXPORT
bool OpenSharedTexture(IDirect3DDevice9* device,
                       int64 surface_handle,
                       const gfx::Size& size,
                       IDirect3DTexture9** opened_texture);

// Create a one-off lockable surface of a specified size.
//
// Returns true on success.
SURFACE_EXPORT
bool CreateTemporaryLockableSurface(IDirect3DDevice9* device,
                                    const gfx::Size& size,
                                    IDirect3DSurface9** surface);

// Create a one-off renderable texture of a specified size. The texture object
// as well as the surface object for the texture's level 0 is returned (callers
// almost always need to use both).
//
// Returns true on success.
SURFACE_EXPORT
bool CreateTemporaryRenderTargetTexture(IDirect3DDevice9* device,
                                        const gfx::Size& size,
                                        IDirect3DTexture9** texture,
                                        IDirect3DSurface9** render_target);

SURFACE_EXPORT
gfx::Size GetSize(IDirect3DTexture9* texture);

SURFACE_EXPORT
gfx::Size GetSize(IDirect3DSurface9* surface);

}  // namespace ui_surface_d3d9_utils

#endif  // UI_SURFACE_D3D9_UTILS_WIN_H_