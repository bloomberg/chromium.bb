// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"

#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ui/gfx/skia_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace {

const uint32_t kShmFormat = WL_SHM_FORMAT_ARGB8888;

}  // namespace

namespace ui {

WaylandShmBuffer::WaylandShmBuffer(wl_shm* shm, const gfx::Size& size)
    : size_(size) {
  Initialize(shm);
}

WaylandShmBuffer::~WaylandShmBuffer() = default;
WaylandShmBuffer::WaylandShmBuffer(WaylandShmBuffer&& buffer) = default;
WaylandShmBuffer& WaylandShmBuffer::operator=(WaylandShmBuffer&& buffer) =
    default;

void WaylandShmBuffer::Initialize(wl_shm* shm) {
  DCHECK(shm);

  SkImageInfo info = SkImageInfo::MakeN32Premul(size_.width(), size_.height());
  int stride = info.minRowBytes();

  size_t buffer_size = info.computeByteSize(stride);
  if (buffer_size == SIZE_MAX)
    return;

  base::UnsafeSharedMemoryRegion shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);
  shared_memory_mapping_ = shared_memory_region.Map();
  if (!shared_memory_mapping_.IsValid()) {
    PLOG(ERROR) << "Create and mmap failed.";
    return;
  }
  base::subtle::PlatformSharedMemoryRegion platform_shared_memory =
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(shared_memory_region));

  wl::Object<wl_shm_pool> pool(wl_shm_create_pool(
      shm, platform_shared_memory.PassPlatformHandle().fd.release(),
      buffer_size));

  auto* new_buffer = wl_shm_pool_create_buffer(
      pool.get(), 0, size_.width(), size_.height(), stride, kShmFormat);
  if (!new_buffer) {
    shared_memory_mapping_ = base::WritableSharedMemoryMapping();
    return;
  }

  stride_ = stride;
  buffer_.reset(new_buffer);
}

uint8_t* WaylandShmBuffer::GetMemory() const {
  if (!IsValid() || !shared_memory_mapping_.IsValid())
    return nullptr;
  return shared_memory_mapping_.GetMemoryAs<uint8_t>();
}

}  // namespace ui
