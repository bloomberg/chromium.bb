// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_buffer.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <xf86drm.h>

#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/ozone/platform/dri/dri_util.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

namespace {

// Modesetting cannot happen from a buffer with transparencies. Return the size
// of a pixel without alpha.
uint8_t GetColorDepth(SkColorType type) {
  switch (type) {
    case kUnknown_SkColorType:
    case kAlpha_8_SkColorType:
      return 0;
    case kIndex_8_SkColorType:
      return 8;
    case kRGB_565_SkColorType:
      return 16;
    case kARGB_4444_SkColorType:
      return 12;
    case kN32_SkColorType:
      return 24;
    default:
      NOTREACHED();
      return 0;
  }
}

void DestroyDumbBuffer(int fd, uint32_t handle) {
  struct drm_mode_destroy_dumb destroy_request;
  memset(&destroy_request, 0, sizeof(destroy_request));
  destroy_request.handle = handle;
  drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_request);
}

bool CreateDumbBuffer(int fd,
                      const SkImageInfo& info,
                      uint32_t* handle,
                      uint32_t* stride) {
  struct drm_mode_create_dumb request;
  memset(&request, 0, sizeof(request));
  request.width = info.width();
  request.height = info.height();
  request.bpp = info.bytesPerPixel() << 3;
  request.flags = 0;

  if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &request) < 0) {
    VLOG(2) << "Cannot create dumb buffer (" << errno << ") "
            << strerror(errno);
    return false;
  }

  // The driver may choose to align the last row as well. We don't care about
  // the last alignment bits since they aren't used for display purposes, so
  // just check that the expected size is <= to what the driver allocated.
  DCHECK_LE(info.getSafeSize(request.pitch), request.size);

  *handle = request.handle;
  *stride = request.pitch;
  return true;
}

}  // namespace

DriBuffer::DriBuffer(DriWrapper* dri)
    : dri_(dri), handle_(0), framebuffer_(0) {}

DriBuffer::~DriBuffer() {
  if (!surface_)
    return;

  if (framebuffer_)
    dri_->RemoveFramebuffer(framebuffer_);

  SkImageInfo info;
  void* pixels = const_cast<void*>(surface_->peekPixels(&info, NULL));
  if (!pixels)
    return;

  munmap(pixels, info.getSafeSize(stride_));
  DestroyDumbBuffer(dri_->get_fd(), handle_);
}

bool DriBuffer::Initialize(const SkImageInfo& info) {
  void* pixels = NULL;
  if (!CreateDumbBuffer(dri_->get_fd(), info, &handle_, &stride_)) {
    VLOG(2) << "Cannot allocate drm dumb buffer";
    return false;
  }

  if (!MapDumbBuffer(dri_->get_fd(),
                     handle_,
                     info.getSafeSize(stride_),
                     &pixels)) {
    VLOG(2) << "Cannot map drm dumb buffer";
    DestroyDumbBuffer(dri_->get_fd(), handle_);
    return false;
  }

  if (!dri_->AddFramebuffer(info.width(),
                            info.height(),
                            GetColorDepth(info.colorType()),
                            info.bytesPerPixel() << 3,
                            stride_,
                            handle_,
                            &framebuffer_)) {
    VLOG(2) << "Failed to register framebuffer: " << strerror(errno);
    return false;
  }

  surface_ = skia::AdoptRef(SkSurface::NewRasterDirect(info, pixels, stride_));
  if (!surface_) {
    VLOG(2) << "Cannot install Skia pixels for drm buffer";
    return false;
  }

  return true;
}

}  // namespace ui
