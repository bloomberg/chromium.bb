// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_buffer.h"

#include <drm.h>
#include <fcntl.h>
#include <gbm.h>
#include <xf86drm.h>
#include <utility>

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
#include "ui/ozone/public/ozone_platform.h"  // nogncheck
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

GbmBuffer::GbmBuffer(const scoped_refptr<GbmDevice>& gbm,
                     gbm_bo* bo,
                     gfx::BufferFormat format,
                     gfx::BufferUsage usage)
    : GbmBufferBase(gbm, bo, format, usage), format_(format), usage_(usage) {}

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

  scoped_refptr<GbmBuffer> buffer(new GbmBuffer(gbm, bo, format, usage));
  if (use_scanout && !buffer->GetFramebufferId())
    return nullptr;

  return buffer;
}

GbmPixmap::GbmPixmap(GbmSurfaceFactory* surface_manager)
    : surface_manager_(surface_manager) {}

void GbmPixmap::Initialize(base::ScopedFD dma_buf, int dma_buf_pitch) {
  dma_buf_ = std::move(dma_buf);
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
  Initialize(std::move(dma_buf), gbm_bo_get_stride(buffer->bo()));
  buffer_ = buffer;
  return true;
}

void GbmPixmap::SetProcessingCallback(
    const ProcessingCallback& processing_callback) {
  DCHECK(processing_callback_.is_null());
  processing_callback_ = processing_callback;
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

void* GbmPixmap::GetEGLClientBuffer() const {
  return nullptr;
}

int GbmPixmap::GetDmaBufFd() const {
  return dma_buf_.get();
}

int GbmPixmap::GetDmaBufPitch() const {
  return dma_buf_pitch_;
}

gfx::BufferFormat GbmPixmap::GetBufferFormat() const {
  return buffer_->GetFormat();
}

gfx::Size GbmPixmap::GetBufferSize() const {
  return buffer_->GetSize();
}

bool GbmPixmap::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                     int plane_z_order,
                                     gfx::OverlayTransform plane_transform,
                                     const gfx::Rect& display_bounds,
                                     const gfx::RectF& crop_rect) {
  // TODO(reveman): Add support for imported buffers. crbug.com/541558
  if (!buffer_) {
    PLOG(ERROR) << "ScheduleOverlayPlane requires a buffer.";
    return false;
  }

  DCHECK(buffer_->GetUsage() == gfx::BufferUsage::SCANOUT);
  surface_manager_->GetSurface(widget)->QueueOverlayPlane(
      OverlayPlane(buffer_, plane_z_order, plane_transform, display_bounds,
                   crop_rect, base::Bind(&GbmPixmap::ProcessBuffer, this)));

  return true;
}

scoped_refptr<ScanoutBuffer> GbmPixmap::ProcessBuffer(const gfx::Size& size,
                                                      uint32_t format) {
  DCHECK(GetBufferSize() != size ||
         buffer_->GetFramebufferPixelFormat() != format);

  if (!processed_pixmap_ || size != processed_pixmap_->GetBufferSize() ||
      format != processed_pixmap_->buffer()->GetFramebufferPixelFormat()) {
    // Release any old processed pixmap.
    processed_pixmap_ = nullptr;
    gfx::BufferFormat buffer_format = GetBufferFormatFromFourCCFormat(format);

    scoped_refptr<GbmBuffer> buffer = GbmBuffer::CreateBuffer(
        buffer_->drm().get(), buffer_format, size, buffer_->GetUsage());

    // ProcessBuffer is called on DrmThread. We could have used
    // CreateNativePixmap to initialize the pixmap, however it posts a
    // synchronous task to DrmThread resulting in a deadlock.
    processed_pixmap_ = new GbmPixmap(surface_manager_);
    if (!processed_pixmap_->InitializeFromBuffer(buffer))
      return nullptr;
  }

  DCHECK(!processing_callback_.is_null());
  if (!processing_callback_.Run(this, processed_pixmap_)) {
    LOG(ERROR) << "Failed processing NativePixmap";
    return nullptr;
  }

  return processed_pixmap_->buffer();
}

}  // namespace ui
