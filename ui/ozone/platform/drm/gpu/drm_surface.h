// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_SURFACE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_SURFACE_H_

#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

class SkSurface;

namespace ui {

class DrmBuffer;
class DrmWindow;
class HardwareDisplayController;

class OZONE_EXPORT DrmSurface : public SurfaceOzoneCanvas {
 public:
  DrmSurface(DrmWindow* window_delegate);
  ~DrmSurface() override;

  // SurfaceOzoneCanvas:
  skia::RefPtr<SkSurface> GetSurface() override;
  void ResizeCanvas(const gfx::Size& viewport_size) override;
  void PresentCanvas(const gfx::Rect& damage) override;
  scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() override;

 private:
  void UpdateNativeSurface(const gfx::Rect& damage);

  DrmWindow* window_delegate_;

  // The actual buffers used for painting.
  scoped_refptr<DrmBuffer> buffers_[2];

  // Keeps track of which bitmap is |buffers_| is the frontbuffer.
  int front_buffer_;

  skia::RefPtr<SkSurface> surface_;
  gfx::Rect last_damage_;

  DISALLOW_COPY_AND_ASSIGN(DrmSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_SURFACE_H_
