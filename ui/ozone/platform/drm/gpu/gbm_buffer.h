// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/gpu/gbm_buffer_base.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_factory_ozone.h"

struct gbm_bo;

namespace ui {

class GbmDevice;

class GbmBuffer : public GbmBufferBase {
 public:
  static scoped_refptr<GbmBuffer> CreateBuffer(
      const scoped_refptr<GbmDevice>& gbm,
      SurfaceFactoryOzone::BufferFormat format,
      const gfx::Size& size,
      SurfaceFactoryOzone::BufferUsage usage);

 private:
  GbmBuffer(const scoped_refptr<GbmDevice>& gbm, gbm_bo* bo, bool scanout);
  ~GbmBuffer() override;

  DISALLOW_COPY_AND_ASSIGN(GbmBuffer);
};

class GbmPixmap : public NativePixmap {
 public:
  GbmPixmap(const scoped_refptr<GbmBuffer>& buffer,
            ScreenManager* screen_manager);
  bool Initialize();
  void SetScalingCallback(const ScalingCallback& scaling_callback) override;
  scoped_refptr<NativePixmap> GetScaledPixmap(gfx::Size new_size) override;

  // NativePixmap:
  void* GetEGLClientBuffer() override;
  int GetDmaBufFd() override;
  int GetDmaBufPitch() override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& crop_rect) override;

  scoped_refptr<GbmBuffer> buffer() { return buffer_; }

 private:
  ~GbmPixmap() override;
  bool ShouldApplyScaling(const gfx::Rect& display_bounds,
                          const gfx::RectF& crop_rect,
                          gfx::Size* required_size);

  scoped_refptr<GbmBuffer> buffer_;
  int dma_buf_ = -1;

  ScreenManager* screen_manager_;  // Not owned.

  ScalingCallback scaling_callback_;

  DISALLOW_COPY_AND_ASSIGN(GbmPixmap);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_H_
