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

// Special DRM implementation of a SkPixelRef. The DRM allocator will create a
// SkPixelRef for the backing pixels. It will then associate the SkPixelRef with
// the SkBitmap. SkBitmap will access the allocated memory by locking the pixels
// in the SkPixelRef.
// At the end of its life the SkPixelRef is responsible for deallocating the
// pixel memory.
class DriSkPixelRef : public SkPixelRef {
 public:
  DriSkPixelRef(const SkImageInfo& info,
                void* pixels,
                SkColorTable* color_table_,
                size_t size,
                size_t row_bites,
                int fd,
                uint32_t handle);
  virtual ~DriSkPixelRef();

  virtual bool onNewLockPixels(LockRec* rec) OVERRIDE;
  virtual void onUnlockPixels() OVERRIDE;

  SK_DECLARE_UNFLATTENABLE_OBJECT()
 private:
  // Raw pointer to the pixel memory.
  void* pixels_;

  // Optional color table associated with the pixel memory.
  SkColorTable* color_table_;

  // Size of the allocated memory.
  size_t size_;

  // Number of bytes between subsequent rows in the bitmap (stride).
  size_t row_bytes_;

  // File descriptor to the graphics card used to allocate/deallocate the
  // memory.
  int fd_;

  // Handle for the allocated memory.
  uint32_t handle_;

  DISALLOW_COPY_AND_ASSIGN(DriSkPixelRef);
};

////////////////////////////////////////////////////////////////////////////////
// DriSkPixelRef implementation

DriSkPixelRef::DriSkPixelRef(
    const SkImageInfo& info,
    void* pixels,
    SkColorTable* color_table,
    size_t size,
    size_t row_bytes,
    int fd,
    uint32_t handle)
  : SkPixelRef(info),
    pixels_(pixels),
    color_table_(color_table),
    size_(size),
    row_bytes_(row_bytes),
    fd_(fd),
    handle_(handle) {
}

DriSkPixelRef::~DriSkPixelRef() {
  munmap(pixels_, size_);
  DestroyDumbBuffer(fd_, handle_);
}

bool DriSkPixelRef::onNewLockPixels(LockRec* rec) {
  rec->fPixels = pixels_;
  rec->fRowBytes = row_bytes_;
  rec->fColorTable = color_table_;
  return true;
}

void DriSkPixelRef::onUnlockPixels() {
}

}  // namespace

// Allocates pixel memory for a SkBitmap using DRM dumb buffers.
class DriAllocator : public SkBitmap::Allocator {
 public:
  DriAllocator();

  virtual bool allocPixelRef(SkBitmap* bitmap,
                             SkColorTable* color_table) OVERRIDE;

 private:
  bool AllocatePixels(DriSkBitmap* bitmap, SkColorTable* color_table);

  DISALLOW_COPY_AND_ASSIGN(DriAllocator);
};

////////////////////////////////////////////////////////////////////////////////
// DriAllocator implementation

DriAllocator::DriAllocator() {
}

bool DriAllocator::allocPixelRef(SkBitmap* bitmap,
                                 SkColorTable* color_table) {
  return AllocatePixels(static_cast<DriSkBitmap*>(bitmap), color_table);
}

bool DriAllocator::AllocatePixels(DriSkBitmap* bitmap,
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

  SkImageInfo info;
  if (!bitmap->asImageInfo(&info)) {
    DLOG(ERROR) << "Cannot get skia image info";
    DestroyDumbBuffer(bitmap->get_fd(), bitmap->get_handle());
    return false;
  }

  bitmap->setPixelRef(new DriSkPixelRef(
      info,
      pixels,
      color_table,
      bitmap->getSize(),
      bitmap->rowBytes(),
      bitmap->get_fd(),
      bitmap->get_handle()))->unref();
  bitmap->lockPixels();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DriSkBitmap implementation

DriSkBitmap::DriSkBitmap(int fd)
  : fd_(fd),
    handle_(0),
    framebuffer_(0) {
}

DriSkBitmap::~DriSkBitmap() {
}

bool DriSkBitmap::Initialize() {
  DriAllocator drm_allocator;
  return allocPixels(&drm_allocator, NULL);
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
