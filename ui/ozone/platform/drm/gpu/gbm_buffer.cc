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
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/gbm_device.h"

namespace ui {

namespace {

int GetGbmFormatFromBufferFormat(gfx::BufferFormat fmt) {
  switch (fmt) {
    case gfx::BufferFormat::BGRA_8888:
      return GBM_FORMAT_ARGB8888;
    case gfx::BufferFormat::BGRX_8888:
      return GBM_FORMAT_XRGB8888;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

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
                             GetGbmFormatFromBufferFormat(format), flags);
  if (!bo)
    return nullptr;

  scoped_refptr<GbmBuffer> buffer(new GbmBuffer(gbm, bo, usage));
  if (use_scanout && !buffer->GetFramebufferId())
    return nullptr;

  return buffer;
}

GbmPixmap::GbmPixmap(const scoped_refptr<GbmBuffer>& buffer,
                     ScreenManager* screen_manager)
    : buffer_(buffer), screen_manager_(screen_manager) {
}

bool GbmPixmap::Initialize() {
  // We want to use the GBM API because it's going to call into libdrm
  // which might do some optimizations on buffer allocation,
  // especially when sharing buffers via DMABUF.
  dma_buf_ = gbm_bo_get_fd(buffer_->bo());
  if (dma_buf_ < 0) {
    PLOG(ERROR) << "Failed to export buffer to dma_buf";
    return false;
  }
  return true;
}

void GbmPixmap::SetScalingCallback(const ScalingCallback& scaling_callback) {
  scaling_callback_ = scaling_callback;
}

scoped_refptr<NativePixmap> GbmPixmap::GetScaledPixmap(gfx::Size new_size) {
  return scaling_callback_.Run(new_size);
}

gfx::NativePixmapHandle GbmPixmap::ExportHandle() {
  gfx::NativePixmapHandle handle;

  int dmabuf_fd = HANDLE_EINTR(dup(dma_buf_));
  if (dmabuf_fd < 0) {
    PLOG(ERROR) << "dup";
    return handle;
  }

  handle.fd = base::FileDescriptor(dmabuf_fd, true /* auto_close */);
  handle.stride = gbm_bo_get_stride(buffer_->bo());
  return handle;
}

GbmPixmap::~GbmPixmap() {
  if (dma_buf_ > 0)
    close(dma_buf_);
}

void* GbmPixmap::GetEGLClientBuffer() {
  return nullptr;
}

int GbmPixmap::GetDmaBufFd() {
  return dma_buf_;
}

int GbmPixmap::GetDmaBufPitch() {
  return gbm_bo_get_stride(buffer_->bo());
}

bool GbmPixmap::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                     int plane_z_order,
                                     gfx::OverlayTransform plane_transform,
                                     const gfx::Rect& display_bounds,
                                     const gfx::RectF& crop_rect) {
  DCHECK(buffer_->GetUsage() == gfx::BufferUsage::SCANOUT);
  gfx::Size required_size;
  if (plane_z_order &&
      ShouldApplyScaling(display_bounds, crop_rect, &required_size)) {
    scoped_refptr<NativePixmap> scaled_pixmap = GetScaledPixmap(required_size);
    if (scaled_pixmap) {
      return scaled_pixmap->ScheduleOverlayPlane(
          widget, plane_z_order, plane_transform, display_bounds, crop_rect);
    } else {
      return false;
    }
  }

  screen_manager_->GetWindow(widget)->QueueOverlayPlane(OverlayPlane(
      buffer_, plane_z_order, plane_transform, display_bounds, crop_rect));
  return true;
}

bool GbmPixmap::ShouldApplyScaling(const gfx::Rect& display_bounds,
                                   const gfx::RectF& crop_rect,
                                   gfx::Size* required_size) {
  if (crop_rect.width() == 0 || crop_rect.height() == 0) {
    PLOG(ERROR) << "ShouldApplyScaling passed zero scaling target.";
    return false;
  }

  gfx::Size pixmap_size = buffer_->GetSize();
  // If the required size is not integer-sized, round it to the next integer.
  *required_size = gfx::ToCeiledSize(
      gfx::SizeF(display_bounds.width() / crop_rect.width(),
                 display_bounds.height() / crop_rect.height()));
  return pixmap_size != *required_size;
}

}  // namespace ui
