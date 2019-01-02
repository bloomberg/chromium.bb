// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_util.h"

#include "base/memory/shared_memory.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/skia_util.h"

namespace wl {

namespace {

const uint32_t kShmFormat = WL_SHM_FORMAT_ARGB8888;
const SkColorType kColorType = kBGRA_8888_SkColorType;

}  // namespace

wl_buffer* CreateSHMBuffer(const gfx::Size& size,
                           base::SharedMemory* shared_memory,
                           wl_shm* shm) {
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  int stride = info.minRowBytes();
  size_t image_buffer_size = info.computeByteSize(stride);
  if (image_buffer_size == SIZE_MAX)
    return nullptr;

  if (shared_memory->handle().GetHandle()) {
    shared_memory->Unmap();
    shared_memory->Close();
  }

  if (!shared_memory->CreateAndMapAnonymous(image_buffer_size)) {
    LOG(ERROR) << "Create and mmap failed.";
    return nullptr;
  }

  // TODO(tonikitoo): Use SharedMemory::requested_size instead of
  // 'image_buffer_size'?
  wl::Object<wl_shm_pool> pool;
  pool.reset(wl_shm_create_pool(shm, shared_memory->handle().GetHandle(),
                                image_buffer_size));
  wl_buffer* buffer = wl_shm_pool_create_buffer(
      pool.get(), 0, size.width(), size.height(), stride, kShmFormat);
  return buffer;
}

void DrawBitmapToSHMB(const gfx::Size& size,
                      const base::SharedMemory& shared_memory,
                      const SkBitmap& bitmap) {
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  int stride = info.minRowBytes();
  sk_sp<SkSurface> sk_surface = SkSurface::MakeRasterDirect(
      SkImageInfo::Make(size.width(), size.height(), kColorType,
                        kOpaque_SkAlphaType),
      static_cast<uint8_t*>(shared_memory.memory()), stride);

  // The |bitmap| contains ARGB image, so update our wl_buffer, which is
  // backed by a SkSurface.
  SkRect damage;
  bitmap.getBounds(&damage);

  // Clear to transparent in case |bitmap| is smaller than the canvas.
  SkCanvas* canvas = sk_surface->getCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  canvas->drawBitmapRect(bitmap, damage, nullptr);
}

}  // namespace wl
