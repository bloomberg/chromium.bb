// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_GBM_SURFACE_H_
#define UI_OZONE_PLATFORM_DRI_GBM_SURFACE_H_

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/dri/gbm_surfaceless.h"
#include "ui/ozone/public/surface_ozone_egl.h"

struct gbm_bo;
struct gbm_device;
struct gbm_surface;

namespace ui {

class DriBuffer;
class DriWrapper;
class DriWindowDelegate;

// Extends the GBM surfaceless functionality and adds an implicit surface for
// the primary plane. Arbitrary buffers can still be allocated and displayed as
// overlay planes, however the primary plane is associated with the native
// surface and is updated via an EGLSurface.
class GbmSurface : public GbmSurfaceless {
 public:
  GbmSurface(DriWindowDelegate* window_delegate,
             gbm_device* device,
             DriWrapper* dri);
  virtual ~GbmSurface();

  bool Initialize();

  // GbmSurfaceless:
  virtual intptr_t GetNativeWindow() OVERRIDE;
  virtual bool ResizeNativeWindow(const gfx::Size& viewport_size) OVERRIDE;
  virtual bool OnSwapBuffers() OVERRIDE;

 private:
  gbm_device* gbm_device_;

  DriWrapper* dri_;

  // The native GBM surface. In EGL this represents the EGLNativeWindowType.
  gbm_surface* native_surface_;

  // Buffer currently used for scanout.
  gbm_bo* current_buffer_;

  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_GBM_SURFACE_H_
