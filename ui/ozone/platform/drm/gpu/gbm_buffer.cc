// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_buffer.h"

#include <drm.h>
#include <fcntl.h>
#include <gbm.h>
#include <xf86drm.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_pixmap_handle_ozone.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/gbm_device.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"

namespace {
// Optimal format for rendering on overlay.
const gfx::BufferFormat kOverlayRenderFormat = gfx::BufferFormat::UYVY_422;
}  // namespace

namespace ui {

GbmBuffer::GbmBuffer(const scoped_refptr<GbmDevice>& gbm,
                     gbm_bo* bo,
                     gfx::BufferUsage usage)
    : GbmBufferBase(gbm, bo, usage == gfx::BufferUsage::SCANOUT),
      usage_(usage) {}

GbmBuffer::~GbmBuffer() {
  if (bo())
    gbm_bo_destroy(bo());
}

// static
scoped_refptr<GbmBuffer> GbmBuffer::CreateBuffer(
    const scoped_refptr<GbmDevice>& gbm,
    gfx::BufferFormat format,
    const gfx::Size& size,
    gfx::BufferUsage usage) {
  TRACE_EVENT2("drm", "GbmBuffer::CreateBuffer", "device",
               gbm->device_path().value(), "size", size.ToString());
  bool use_scanout = (usage == gfx::BufferUsage::SCANOUT);
  unsigned flags = 0;
  // GBM_BO_USE_SCANOUT is the hint of x-tiling.
  if (use_scanout)
    flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
  gbm_bo* bo = gbm_bo_create(gbm->device(), size.width(), size.height(),
                             GetFourCCFormatFromBufferFormat(format), flags);
  if (!bo)
    return nullptr;

  scoped_refptr<GbmBuffer> buffer(new GbmBuffer(gbm, bo, usage));
  if (use_scanout && !buffer->GetFramebufferId())
    return nullptr;

  return buffer;
}

GbmPixmap::GbmPixmap(GbmSurfaceFactory* surface_manager)
    : surface_manager_(surface_manager) {}

void GbmPixmap::Initialize(base::ScopedFD dma_buf, int dma_buf_pitch) {
  dma_buf_ = dma_buf.Pass();
  dma_buf_pitch_ = dma_buf_pitch;
}

bool GbmPixmap::InitializeFromBuffer(const scoped_refptr<GbmBuffer>& buffer) {
  // We want to use the GBM API because it's going to call into libdrm
  // which might do some optimizations on buffer allocation,
  // especially when sharing buffers via DMABUF.
  base::ScopedFD dma_buf(gbm_bo_get_fd(buffer->bo()));
  if (!dma_buf.is_valid()) {
    PLOG(ERROR) << "Failed to export buffer to dma_buf";
    return false;
  }
  Initialize(dma_buf.Pass(), gbm_bo_get_stride(buffer->bo()));
  buffer_ = buffer;
  return true;
}

void GbmPixmap::SetProcessingCallback(
    const ProcessingCallback& processing_callback) {
  processing_callback_ = processing_callback;
}

scoped_refptr<NativePixmap> GbmPixmap::GetProcessedPixmap(
    gfx::Size target_size,
    gfx::BufferFormat target_format) {
  return processing_callback_.Run(target_size, target_format);
}

gfx::NativePixmapHandle GbmPixmap::ExportHandle() {
  gfx::NativePixmapHandle handle;

  base::ScopedFD dmabuf_fd(HANDLE_EINTR(dup(dma_buf_.get())));
  if (!dmabuf_fd.is_valid()) {
    PLOG(ERROR) << "dup";
    return handle;
  }

  handle.fd = base::FileDescriptor(dmabuf_fd.release(), true /* auto_close */);
  handle.stride = dma_buf_pitch_;
  return handle;
}

GbmPixmap::~GbmPixmap() {
}

void* GbmPixmap::GetEGLClientBuffer() {
  return nullptr;
}

int GbmPixmap::GetDmaBufFd() {
  return dma_buf_.get();
}

int GbmPixmap::GetDmaBufPitch() {
  return dma_buf_pitch_;
}

gfx::BufferFormat GbmPixmap::GetBufferFormat() {
  return GetBufferFormatFromFourCCFormat(buffer_->GetFramebufferPixelFormat());
}

bool GbmPixmap::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                     int plane_z_order,
                                     gfx::OverlayTransform plane_transform,
                                     const gfx::Rect& display_bounds,
                                     const gfx::RectF& crop_rect) {
  gfx::Size target_size;
  gfx::BufferFormat target_format;
  if (plane_z_order && ShouldApplyProcessing(display_bounds, crop_rect,
                                             &target_size, &target_format)) {
    scoped_refptr<NativePixmap> processed_pixmap =
        GetProcessedPixmap(target_size, target_format);
    if (processed_pixmap) {
      return processed_pixmap->ScheduleOverlayPlane(
          widget, plane_z_order, plane_transform, display_bounds, crop_rect);
    } else {
      return false;
    }
  }

  // TODO(reveman): Add support for imported buffers. crbug.com/541558
  if (!buffer_) {
    PLOG(ERROR) << "ScheduleOverlayPlane requires a buffer.";
    return false;
  }

  DCHECK(buffer_->GetUsage() == gfx::BufferUsage::SCANOUT);
  surface_manager_->GetSurface(widget)->QueueOverlayPlane(OverlayPlane(
      buffer_, plane_z_order, plane_transform, display_bounds, crop_rect));
  return true;
}

bool GbmPixmap::ShouldApplyProcessing(const gfx::Rect& display_bounds,
                                      const gfx::RectF& crop_rect,
                                      gfx::Size* target_size,
                                      gfx::BufferFormat* target_format) {
  if (crop_rect.width() == 0 || crop_rect.height() == 0) {
    PLOG(ERROR) << "ShouldApplyProcessing passed zero processing target.";
    return false;
  }

  if (!buffer_) {
    PLOG(ERROR) << "ShouldApplyProcessing requires a buffer.";
    return false;
  }

  // TODO(william.xie): Figure out the optimal render format for overlay.
  // See http://crbug.com/553264.
  *target_format = kOverlayRenderFormat;
  gfx::Size pixmap_size = buffer_->GetSize();
  // If the required size is not integer-sized, round it to the next integer.
  *target_size = gfx::ToCeiledSize(
      gfx::SizeF(display_bounds.width() / crop_rect.width(),
                 display_bounds.height() / crop_rect.height()));

  return pixmap_size != *target_size || GetBufferFormat() != *target_format;
}

}  // namespace ui
