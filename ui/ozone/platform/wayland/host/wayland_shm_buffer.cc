// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"

#include "ui/gfx/skia_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace {

const uint32_t kShmFormat = WL_SHM_FORMAT_ARGB8888;

}  // namespace

namespace ui {

WaylandShmBuffer::WaylandShmBuffer(wl_shm* shm, const gfx::Size& size)
    : size_(size), shared_memory_(std::make_unique<base::SharedMemory>()) {
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

  if (!shared_memory_->CreateAndMapAnonymous(buffer_size)) {
    PLOG(ERROR) << "Create and mmap failed.";
    return;
  }

  auto handle = shared_memory_->handle().GetHandle();
  wl::Object<wl_shm_pool> pool(wl_shm_create_pool(shm, handle, buffer_size));

  auto* new_buffer = wl_shm_pool_create_buffer(
      pool.get(), 0, size_.width(), size_.height(), stride, kShmFormat);
  if (!new_buffer) {
    shared_memory_->Unmap();
    shared_memory_->Close();
    return;
  }

  stride_ = stride;
  buffer_.reset(new_buffer);
}

uint8_t* WaylandShmBuffer::GetMemory() const {
  if (!IsValid() || !shared_memory_)
    return nullptr;
  return static_cast<uint8_t*>(shared_memory_->memory());
}

}  // namespace ui
