// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ozone/impl/drm_skbitmap_ozone.h"

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

// Special DRM implementation of a SkPixelRef. The DRM allocator will create a
// SkPixelRef for the backing pixels. It will then associate the SkPixelRef with
// the SkBitmap. SkBitmap will access the allocated memory by locking the pixels
// in the SkPixelRef.
// At the end of its life the SkPixelRef is responsible for deallocating the
// pixel memory.
class DrmSkPixelRef : public SkPixelRef {
 public:
  DrmSkPixelRef(void* pixels,
                SkColorTable* color_table_,
                size_t size,
                int fd,
                uint32_t handle);
  virtual ~DrmSkPixelRef();

  virtual void* onLockPixels(SkColorTable** ct) OVERRIDE;
  virtual void onUnlockPixels() OVERRIDE;

  SK_DECLARE_UNFLATTENABLE_OBJECT()
 private:
  // Raw pointer to the pixel memory.
  void* pixels_;

  // Optional color table associated with the pixel memory.
  SkColorTable* color_table_;

  // Size of the allocated memory.
  size_t size_;

  // File descriptor to the graphics card used to allocate/deallocate the
  // memory.
  int fd_;

  // Handle for the allocated memory.
  uint32_t handle_;

  DISALLOW_COPY_AND_ASSIGN(DrmSkPixelRef);
};

////////////////////////////////////////////////////////////////////////////////
// DrmSkPixelRef implementation

DrmSkPixelRef::DrmSkPixelRef(
    void* pixels,
    SkColorTable* color_table,
    size_t size,
    int fd,
    uint32_t handle)
  : pixels_(pixels),
    color_table_(color_table),
    size_(size),
    fd_(fd),
    handle_(handle) {
}

DrmSkPixelRef::~DrmSkPixelRef() {
  munmap(pixels_, size_);
  DestroyDumbBuffer(fd_, handle_);
}

void* DrmSkPixelRef::onLockPixels(SkColorTable** ct) {
  *ct = color_table_;
  return pixels_;
}

void DrmSkPixelRef::onUnlockPixels() {
}

}  // namespace

// Allocates pixel memory for a SkBitmap using DRM dumb buffers.
class DrmAllocator : public SkBitmap::Allocator {
 public:
  DrmAllocator();

  virtual bool allocPixelRef(SkBitmap* bitmap,
                             SkColorTable* color_table) OVERRIDE;

 private:
  bool AllocatePixels(DrmSkBitmapOzone* bitmap, SkColorTable* color_table);

  DISALLOW_COPY_AND_ASSIGN(DrmAllocator);
};

////////////////////////////////////////////////////////////////////////////////
// DrmAllocator implementation

DrmAllocator::DrmAllocator() {
}

bool DrmAllocator::allocPixelRef(SkBitmap* bitmap,
                                 SkColorTable* color_table) {
  return AllocatePixels(static_cast<DrmSkBitmapOzone*>(bitmap), color_table);
}

bool DrmAllocator::AllocatePixels(DrmSkBitmapOzone* bitmap,
                                  SkColorTable* color_table) {
  struct drm_mode_create_dumb request;
  request.width = bitmap->width();
  request.height = bitmap->height();
  request.bpp = bitmap->bytesPerPixel() << 3;
  request.flags = 0;

  if (drmIoctl(bitmap->get_fd(), DRM_IOCTL_MODE_CREATE_DUMB, &request) < 0) {
    DLOG(ERROR) << "Cannot create dumb buffer (" << errno << ") "
                << strerror(errno);
    return false;
  }

  CHECK(request.size == bitmap->getSize());

  bitmap->set_handle(request.handle);

  struct drm_mode_map_dumb map_request;
  map_request.handle = bitmap->get_handle();
  if (drmIoctl(bitmap->get_fd(), DRM_IOCTL_MODE_MAP_DUMB, &map_request)) {
    DLOG(ERROR) << "Cannot prepare dumb buffer for mapping (" << errno << ") "
                << strerror(errno);
    DestroyDumbBuffer(bitmap->get_fd(), bitmap->get_handle());
    return false;
  }

  void* pixels = mmap(0,
                      bitmap->getSize(),
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED,
                      bitmap->get_fd(),
                      map_request.offset);
  if (pixels == MAP_FAILED) {
    DLOG(ERROR) << "Cannot mmap dumb buffer (" << errno << ") "
                << strerror(errno);
    DestroyDumbBuffer(bitmap->get_fd(), bitmap->get_handle());
    return false;
  }

  bitmap->setPixelRef(new DrmSkPixelRef(
      pixels,
      color_table,
      bitmap->getSize(),
      bitmap->get_fd(),
      bitmap->get_handle()))->unref();
  bitmap->lockPixels();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DrmSkBitmapOzone implementation

DrmSkBitmapOzone::DrmSkBitmapOzone(int fd)
  : fd_(fd),
    handle_(0),
    framebuffer_(0) {
}

DrmSkBitmapOzone::~DrmSkBitmapOzone() {
}

bool DrmSkBitmapOzone::Initialize() {
  DrmAllocator drm_allocator;
  return allocPixels(&drm_allocator, NULL);
}

uint8_t DrmSkBitmapOzone::GetColorDepth() const {
  switch (config()) {
    case SkBitmap::kNo_Config:
    case SkBitmap::kA1_Config:
    case SkBitmap::kA8_Config:
      return 0;
    case SkBitmap::kIndex8_Config:
      return 8;
    case SkBitmap::kRGB_565_Config:
      return 16;
    case SkBitmap::kARGB_4444_Config:
      return 12;
    case SkBitmap::kARGB_8888_Config:
      return 24;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace gfx
