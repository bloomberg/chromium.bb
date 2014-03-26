// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ozone/dri/dri_skbitmap.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <xf86drm.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkPixelRef.h"

namespace gfx {

namespace {

void DestroyDumbBuffer(int fd, uint32_t handle) {
  struct drm_mode_destroy_dumb destroy_request;
  destroy_request.handle = handle;
  drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_request);
}

bool CreateDumbBuffer(
    DriSkBitmap* bitmap,
    const SkImageInfo& info,
    size_t* stride,
    void** pixels) {
  struct drm_mode_create_dumb request;
  request.width = info.width();
  request.height = info.height();
  request.bpp = info.bytesPerPixel() << 3;
  request.flags = 0;

  if (drmIoctl(bitmap->get_fd(), DRM_IOCTL_MODE_CREATE_DUMB, &request) < 0) {
    DLOG(ERROR) << "Cannot create dumb buffer (" << errno << ") "
                << strerror(errno);
    return false;
  }

  bitmap->set_handle(request.handle);
  *stride = request.pitch;

  struct drm_mode_map_dumb map_request;
  map_request.handle = bitmap->get_handle();
  if (drmIoctl(bitmap->get_fd(), DRM_IOCTL_MODE_MAP_DUMB, &map_request)) {
    DLOG(ERROR) << "Cannot prepare dumb buffer for mapping (" << errno << ") "
                << strerror(errno);
    DestroyDumbBuffer(bitmap->get_fd(), bitmap->get_handle());
    return false;
  }

  *pixels = mmap(0,
                 request.size,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED,
                 bitmap->get_fd(),
                 map_request.offset);
  if (*pixels == MAP_FAILED) {
    DLOG(ERROR) << "Cannot mmap dumb buffer (" << errno << ") "
                << strerror(errno);
    DestroyDumbBuffer(bitmap->get_fd(), bitmap->get_handle());
    return false;
  }

  return true;
}

void ReleasePixels(void* addr, void* context) {
  DriSkBitmap *bitmap = reinterpret_cast<DriSkBitmap*>(context);
  if (!bitmap && !bitmap->getPixels())
    return;

  munmap(bitmap->getPixels(), bitmap->getSize());
  DestroyDumbBuffer(bitmap->get_fd(), bitmap->get_handle());
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DriSkBitmap implementation

DriSkBitmap::DriSkBitmap(int fd)
  : fd_(fd),
    handle_(0),
    framebuffer_(0) {
}

DriSkBitmap::~DriSkBitmap() {
}

bool DriSkBitmap::Initialize(const SkImageInfo& info) {
  size_t stride = 0;
  void* pixels = NULL;
  if (!CreateDumbBuffer(this, info, &stride, &pixels)) {
    DLOG(ERROR) << "Cannot allocate drm dumb buffer";
    return false;
  }
  if (!installPixels(info, pixels, stride, ReleasePixels, this)) {
    DLOG(ERROR) << "Cannot install Skia pixels for drm buffer";
    return false;
  }

  return true;
}

uint8_t DriSkBitmap::GetColorDepth() const {
  switch (colorType()) {
    case kUnknown_SkColorType:
    case kAlpha_8_SkColorType:
      return 0;
    case kIndex_8_SkColorType:
      return 8;
    case kRGB_565_SkColorType:
      return 16;
    case kARGB_4444_SkColorType:
      return 12;
    case kPMColor_SkColorType:
      return 24;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace gfx
