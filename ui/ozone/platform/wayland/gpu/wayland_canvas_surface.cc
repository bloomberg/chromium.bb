// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_canvas_surface.h"

#include <memory>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/platform/wayland/gpu/wayland_connection_proxy.h"

namespace ui {

namespace {

void DeleteSharedMemoryMapping(void* pixels, void* context) {
  delete static_cast<base::WritableSharedMemoryMapping*>(context);
}

}  // namespace

WaylandCanvasSurface::WaylandCanvasSurface(WaylandConnectionProxy* connection,
                                           gfx::AcceleratedWidget widget)
    : connection_(connection), widget_(widget) {}

WaylandCanvasSurface::~WaylandCanvasSurface() {
  if (sk_surface_)
    connection_->DestroyShmBuffer(widget_);
}

sk_sp<SkSurface> WaylandCanvasSurface::GetSurface() {
  if (sk_surface_)
    return sk_surface_;
  DCHECK(!size_.IsEmpty());

  size_t length = size_.width() * size_.height() * 4;
  base::UnsafeSharedMemoryRegion shm_region =
      base::UnsafeSharedMemoryRegion::Create(length);
  if (!shm_region.IsValid())
    return nullptr;

  base::WritableSharedMemoryMapping shm_mapping = shm_region.Map();
  if (!shm_mapping.IsValid())
    return nullptr;

  base::subtle::PlatformSharedMemoryRegion platform_shm =
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(shm_region));
  base::subtle::ScopedFDPair fd_pair = platform_shm.PassPlatformHandle();
  base::File file(fd_pair.fd.release());
  connection_->CreateShmBufferForWidget(widget_, std::move(file), length,
                                        size_);

  auto shm_mapping_on_heap =
      std::make_unique<base::WritableSharedMemoryMapping>(
          std::move(shm_mapping));
  sk_surface_ = SkSurface::MakeRasterDirectReleaseProc(
      SkImageInfo::MakeN32Premul(size_.width(), size_.height()),
      shm_mapping_on_heap->memory(), size_.width() * 4,
      &DeleteSharedMemoryMapping, shm_mapping_on_heap.get(), nullptr);
  if (!sk_surface_)
    return nullptr;

  ignore_result(shm_mapping_on_heap.release());
  return sk_surface_;
}

void WaylandCanvasSurface::ResizeCanvas(const gfx::Size& viewport_size) {
  if (size_ == viewport_size)
    return;
  // TODO(https://crbug.com/930667): We could implement more efficient resizes
  // by allocating buffers rounded up to larger sizes, and then reusing them if
  // the new size still fits (but still reallocate if the new size is much
  // smaller than the old size).
  if (sk_surface_) {
    sk_surface_.reset();
    connection_->DestroyShmBuffer(widget_);
  }
  size_ = viewport_size;
}

void WaylandCanvasSurface::PresentCanvas(const gfx::Rect& damage) {
  connection_->PresentShmBufferForWidget(widget_, damage);
}

std::unique_ptr<gfx::VSyncProvider>
WaylandCanvasSurface::CreateVSyncProvider() {
  // TODO(https://crbug.com/930662): This can be implemented with information
  // from frame callbacks, and possibly output refresh rate.
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

}  // namespace ui
