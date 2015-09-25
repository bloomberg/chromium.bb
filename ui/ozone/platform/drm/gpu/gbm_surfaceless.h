// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_

#include <vector>

#include "ui/ozone/platform/drm/gpu/overlay_plane.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace ui {

class DrmDeviceManager;
class DrmWindow;
class GbmSurfaceFactory;

// In surfaceless mode drawing and displaying happens directly through
// NativePixmap buffers. CC would call into SurfaceFactoryOzone to allocate the
// buffers and then call ScheduleOverlayPlane(..) to schedule the buffer for
// presentation.
class GbmSurfaceless : public SurfaceOzoneEGL {
 public:
  GbmSurfaceless(DrmWindow* window,
                 DrmDeviceManager* drm_device_manager,
                 GbmSurfaceFactory* surface_manager);
  ~GbmSurfaceless() override;

  void QueueOverlayPlane(const OverlayPlane& plane);

  // SurfaceOzoneEGL:
  intptr_t GetNativeWindow() override;
  bool ResizeNativeWindow(const gfx::Size& viewport_size) override;
  bool OnSwapBuffers() override;
  bool OnSwapBuffersAsync(const SwapCompletionCallback& callback) override;
  scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;
  bool IsUniversalDisplayLinkDevice() override;

 protected:
  DrmWindow* window_;
  DrmDeviceManager* drm_device_manager_;
  GbmSurfaceFactory* surface_manager_;
  std::vector<OverlayPlane> planes_;

  DISALLOW_COPY_AND_ASSIGN(GbmSurfaceless);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_SURFACELESS_H_
