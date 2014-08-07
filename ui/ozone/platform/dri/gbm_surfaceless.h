// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_GBM_SURFACELESS_H_
#define UI_OZONE_PLATFORM_DRI_GBM_SURFACELESS_H_

#include "base/memory/scoped_ptr.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace gfx {
class Size;
class Rect;
}  // namespace gfx

namespace ui {

// In surfaceless mode drawing and displaying happens directly through
// NativePixmap buffers. CC would call into SurfaceFactoryOzone to allocate the
// buffers and then call ScheduleOverlayPlane(..) to schedule the buffer for
// presentation.
class GbmSurfaceless : public SurfaceOzoneEGL {
 public:
  GbmSurfaceless(const base::WeakPtr<HardwareDisplayController>& controller);
  virtual ~GbmSurfaceless();

  // SurfaceOzoneEGL:
  virtual intptr_t GetNativeWindow() OVERRIDE;
  virtual bool ResizeNativeWindow(const gfx::Size& viewport_size) OVERRIDE;
  virtual bool OnSwapBuffers() OVERRIDE;
  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() OVERRIDE;
  virtual bool ScheduleOverlayPlane(int plane_z_order,
                                    gfx::OverlayTransform plane_transform,
                                    scoped_refptr<ui::NativePixmap> buffer,
                                    const gfx::Rect& display_bounds,
                                    const gfx::RectF& crop_rect) OVERRIDE;

 protected:
  base::WeakPtr<HardwareDisplayController> controller_;
  OverlayPlaneList queued_planes_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceless);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_GBM_SURFACELESS_H_
