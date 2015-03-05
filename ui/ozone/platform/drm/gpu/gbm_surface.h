// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"
#include "ui/ozone/public/surface_ozone_egl.h"

struct gbm_bo;
struct gbm_surface;

namespace ui {

class DrmBuffer;
class DrmWindow;
class GbmDevice;

// Extends the GBM surfaceless functionality and adds an implicit surface for
// the primary plane. Arbitrary buffers can still be allocated and displayed as
// overlay planes, however the primary plane is associated with the native
// surface and is updated via an EGLSurface.
class GbmSurface : public GbmSurfaceless {
 public:
  GbmSurface(DrmWindow* window_delegate, const scoped_refptr<GbmDevice>& gbm);
  ~GbmSurface() override;

  bool Initialize();

  // GbmSurfaceless:
  intptr_t GetNativeWindow() override;
  bool ResizeNativeWindow(const gfx::Size& viewport_size) override;
  bool OnSwapBuffers() override;
  bool OnSwapBuffersAsync(const SwapCompletionCallback& callback) override;

 private:
  void OnSwapBuffersCallback(const SwapCompletionCallback& callback,
                             gbm_bo* pending_buffer);

  scoped_refptr<GbmDevice> gbm_;

  // The native GBM surface. In EGL this represents the EGLNativeWindowType.
  gbm_surface* native_surface_;

  // Buffer currently used for scanout.
  gbm_bo* current_buffer_;

  gfx::Size size_;

  base::WeakPtrFactory<GbmSurface> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACE_H_
