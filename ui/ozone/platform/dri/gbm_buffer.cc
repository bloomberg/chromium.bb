// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_buffer.h"

#include <drm.h>
#include <fcntl.h>
#include <gbm.h>
#include <xf86drm.h>

#include "base/logging.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

namespace {

int GetGbmFormatFromBufferFormat(SurfaceFactoryOzone::BufferFormat fmt) {
  switch (fmt) {
    case SurfaceFactoryOzone::RGBA_8888:
      return GBM_BO_FORMAT_ARGB8888;
    case SurfaceFactoryOzone::RGBX_8888:
      return GBM_BO_FORMAT_XRGB8888;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

GbmBuffer::GbmBuffer(DriWrapper* dri, gbm_bo* bo, bool scanout)
    : GbmBufferBase(dri, bo, scanout) {
}

GbmBuffer::~GbmBuffer() {
  if (bo())
    gbm_bo_destroy(bo());
}

// static
scoped_refptr<GbmBuffer> GbmBuffer::CreateBuffer(
    DriWrapper* dri,
    gbm_device* device,
    SurfaceFactoryOzone::BufferFormat format,
    const gfx::Size& size,
    bool scanout) {
  unsigned flags = GBM_BO_USE_RENDERING;
  if (scanout)
    flags |= GBM_BO_USE_SCANOUT;
  gbm_bo* bo = gbm_bo_create(device,
                             size.width(),
                             size.height(),
                             GetGbmFormatFromBufferFormat(format),
                             flags);
  if (!bo)
    return NULL;

  scoped_refptr<GbmBuffer> buffer(new GbmBuffer(dri, bo, scanout));
  if (scanout && !buffer->GetFramebufferId())
    return NULL;

  return buffer;
}

GbmPixmap::GbmPixmap(scoped_refptr<GbmBuffer> buffer)
    : buffer_(buffer), dma_buf_(-1) {
}

bool GbmPixmap::Initialize(DriWrapper* dri) {
  if (drmPrimeHandleToFD(dri->get_fd(), buffer_->GetHandle(), DRM_CLOEXEC,
                         &dma_buf_)) {
    LOG(ERROR) << "Failed to export buffer to dma_buf";
    return false;
  }

  return true;
}

GbmPixmap::~GbmPixmap() {
  if (dma_buf_ > 0)
    close(dma_buf_);
}

void* GbmPixmap::GetEGLClientBuffer() {
  return NULL;
}

int GbmPixmap::GetDmaBufFd() {
  return dma_buf_;
}

int GbmPixmap::GetDmaBufPitch() {
  return gbm_bo_get_stride(buffer_->bo());
}

}  // namespace ui
