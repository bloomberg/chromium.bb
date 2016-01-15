// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_H_

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/gpu/gbm_buffer_base.h"
#include "ui/ozone/public/native_pixmap.h"

struct gbm_bo;

namespace ui {

class GbmDevice;
class GbmSurfaceFactory;

class GbmBuffer : public GbmBufferBase {
 public:
  static scoped_refptr<GbmBuffer> CreateBuffer(
      const scoped_refptr<GbmDevice>& gbm,
      gfx::BufferFormat format,
      const gfx::Size& size,
      gfx::BufferUsage usage);
  gfx::BufferFormat GetFormat() const { return format_; }
  gfx::BufferUsage GetUsage() const { return usage_; }

 private:
  GbmBuffer(const scoped_refptr<GbmDevice>& gbm,
            gbm_bo* bo,
            gfx::BufferFormat format,
            gfx::BufferUsage usage);
  ~GbmBuffer() override;

  gfx::BufferFormat format_;
  gfx::BufferUsage usage_;

  DISALLOW_COPY_AND_ASSIGN(GbmBuffer);
};

class GbmPixmap : public NativePixmap {
 public:
  explicit GbmPixmap(GbmSurfaceFactory* surface_manager);
  void Initialize(base::ScopedFD dma_buf, int dma_buf_pitch);
  bool InitializeFromBuffer(const scoped_refptr<GbmBuffer>& buffer);
  void SetProcessingCallback(
      const ProcessingCallback& processing_callback) override;

  // NativePixmap:
  void* GetEGLClientBuffer() const override;
  int GetDmaBufFd() const override;
  int GetDmaBufPitch() const override;
  gfx::BufferFormat GetBufferFormat() const override;
  gfx::Size GetBufferSize() const override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& crop_rect) override;
  gfx::NativePixmapHandle ExportHandle() override;

  scoped_refptr<GbmBuffer> buffer() { return buffer_; }

 private:
  ~GbmPixmap() override;
  scoped_refptr<ScanoutBuffer> ProcessBuffer(const gfx::Size& size,
                                             uint32_t format);

  scoped_refptr<GbmBuffer> buffer_;
  base::ScopedFD dma_buf_;
  int dma_buf_pitch_ = -1;

  GbmSurfaceFactory* surface_manager_;

  // OverlayValidator can request scaling or format conversions as needed for
  // this Pixmap. This holds the processed buffer.
  scoped_refptr<GbmPixmap> processed_pixmap_;
  ProcessingCallback processing_callback_;

  DISALLOW_COPY_AND_ASSIGN(GbmPixmap);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_BUFFER_H_
