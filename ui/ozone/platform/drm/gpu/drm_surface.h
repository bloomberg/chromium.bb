// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_SURFACE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_SURFACE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

class SkImage;
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
  void SchedulePageFlip();

  // Callback for SchedulePageFlip(). This will signal when the page flip event
  // has completed.
  void OnPageFlip(gfx::SwapResult result);

  DrmWindow* window_delegate_;

  // The actual buffers used for painting.
  scoped_refptr<DrmBuffer> front_buffer_;
  scoped_refptr<DrmBuffer> back_buffer_;

  skia::RefPtr<SkSurface> surface_;
  gfx::Rect last_damage_;

  // Keep track of the requested image and damage for the last presentation.
  // This will be used to update the scanout buffers once the previous page flip
  // events completes.
  skia::RefPtr<SkImage> pending_image_;
  gfx::Rect pending_image_damage_;
  bool pending_pageflip_ = false;

  base::WeakPtrFactory<DrmSurface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DrmSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_SURFACE_H_
