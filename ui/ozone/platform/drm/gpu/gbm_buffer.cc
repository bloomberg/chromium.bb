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
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

GbmBuffer::GbmBuffer(const scoped_refptr<GbmDevice>& gbm,
                     gbm_bo* bo,
                     gfx::BufferFormat format,
                     gfx::BufferUsage usage,
                     base::ScopedFD fd,
                     const gfx::Size& size,
                     int stride)
    : GbmBufferBase(gbm, bo, format, usage),
      format_(format),
      usage_(usage),
      fd_(std::move(fd)),
      size_(size),
      stride_(stride) {}

GbmBuffer::~GbmBuffer() {
  if (bo())
    gbm_bo_destroy(bo());
}

int GbmBuffer::GetFd() const {
  return fd_.get();
}

int GbmBuffer::GetStride() const {
  return stride_;
}

// TODO(reveman): This should not be needed once crbug.com/597932 is fixed,
// as the size would be queried directly from the underlying bo.
gfx::Size GbmBuffer::GetSize() const {
  return size_;
}

// static
scoped_refptr<GbmBuffer> GbmBuffer::CreateBuffer(
    const scoped_refptr<GbmDevice>& gbm,
    gfx::BufferFormat format,
    const gfx::Size& size,
    gfx::BufferUsage usage) {
  TRACE_EVENT2("drm", "GbmBuffer::CreateBuffer", "device",
               gbm->device_path().value(), "size", size.ToString());

  unsigned flags = 0;
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      break;
    case gfx::BufferUsage::SCANOUT:
      flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
      break;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      flags = GBM_BO_USE_LINEAR;
      break;
  }

  gbm_bo* bo = gbm_bo_create(gbm->device(), size.width(), size.height(),
                             GetFourCCFormatFromBufferFormat(format), flags);
  if (!bo)
    return nullptr;

  // The fd returned by gbm_bo_get_fd is not ref-counted and need to be
  // kept open for the lifetime of the buffer.
  base::ScopedFD fd(gbm_bo_get_fd(bo));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to export buffer to dma_buf";
    gbm_bo_destroy(bo);
    return nullptr;
  }

  scoped_refptr<GbmBuffer> buffer(new GbmBuffer(
      gbm, bo, format, usage, std::move(fd), size, gbm_bo_get_stride(bo)));
  if (usage == gfx::BufferUsage::SCANOUT && !buffer->GetFramebufferId())
    return nullptr;

  return buffer;
}

// static
scoped_refptr<GbmBuffer> GbmBuffer::CreateBufferFromFD(
    const scoped_refptr<GbmDevice>& gbm,
    gfx::BufferFormat format,
    const gfx::Size& size,
    base::ScopedFD fd,
    int stride) {
  TRACE_EVENT2("drm", "GbmBuffer::CreateBufferFromFD", "device",
               gbm->device_path().value(), "size", size.ToString());

  // TODO(reveman): Use gbm_bo_import after making buffers survive
  // GPU process crashes. crbug.com/597932
  return make_scoped_refptr(new GbmBuffer(gbm, nullptr, format,
                                          gfx::BufferUsage::GPU_READ,
                                          std::move(fd), size, stride));
}

GbmPixmap::GbmPixmap(GbmSurfaceFactory* surface_manager,
                     const scoped_refptr<GbmBuffer>& buffer)
    : surface_manager_(surface_manager), buffer_(buffer) {}

void GbmPixmap::SetProcessingCallback(
    const ProcessingCallback& processing_callback) {
  DCHECK(processing_callback_.is_null());
  processing_callback_ = processing_callback;
}

gfx::NativePixmapHandle GbmPixmap::ExportHandle() {
  gfx::NativePixmapHandle handle;

  base::ScopedFD fd(HANDLE_EINTR(dup(buffer_->GetFd())));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "dup";
    return handle;
  }

  handle.fd = base::FileDescriptor(fd.release(), true /* auto_close */);
  handle.stride = buffer_->GetStride();
  return handle;
}

GbmPixmap::~GbmPixmap() {
}

void* GbmPixmap::GetEGLClientBuffer() const {
  return nullptr;
}

int GbmPixmap::GetDmaBufFd() const {
  return buffer_->GetFd();
}

int GbmPixmap::GetDmaBufPitch() const {
  return buffer_->GetStride();
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
  DCHECK(buffer_->GetUsage() == gfx::BufferUsage::SCANOUT);
  OverlayPlane::ProcessBufferCallback processing_callback;
  if (!processing_callback_.is_null())
    processing_callback = base::Bind(&GbmPixmap::ProcessBuffer, this);

  surface_manager_->GetSurface(widget)->QueueOverlayPlane(
      OverlayPlane(buffer_, plane_z_order, plane_transform, display_bounds,
                   crop_rect, processing_callback));

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
    if (!buffer)
      return nullptr;

    // ProcessBuffer is called on DrmThread. We could have used
    // CreateNativePixmap to initialize the pixmap, however it posts a
    // synchronous task to DrmThread resulting in a deadlock.
    processed_pixmap_ = new GbmPixmap(surface_manager_, buffer);
  }

  DCHECK(!processing_callback_.is_null());
  if (!processing_callback_.Run(this, processed_pixmap_)) {
    LOG(ERROR) << "Failed processing NativePixmap";
    return nullptr;
  }

  return processed_pixmap_->buffer();
}

}  // namespace ui
