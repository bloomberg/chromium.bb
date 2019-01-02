// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_cursor.h"

#include <sys/mman.h>
#include <vector>

#include "base/memory/shared_memory.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_pointer.h"
#include "ui/ozone/platform/wayland/wayland_util.h"

namespace ui {

WaylandCursor::WaylandCursor() : shared_memory_(new base::SharedMemory()) {}

void WaylandCursor::Init(wl_pointer* pointer, WaylandConnection* connection) {
  if (input_pointer_ == pointer)
    return;

  input_pointer_ = pointer;

  DCHECK(connection);
  shm_ = connection->shm();
  pointer_surface_.reset(
      wl_compositor_create_surface(connection->compositor()));
}

WaylandCursor::~WaylandCursor() {
  pointer_surface_.reset();
  buffer_.reset();

  if (shared_memory_->handle().GetHandle()) {
    shared_memory_->Unmap();
    shared_memory_->Close();
  }
}

void WaylandCursor::UpdateBitmap(const std::vector<SkBitmap>& cursor_image,
                                 const gfx::Point& location,
                                 uint32_t serial) {
  if (!input_pointer_)
    return;

  if (!cursor_image.size()) {
    HideCursor(serial);
    return;
  }

  const SkBitmap& image = cursor_image[0];
  SkISize size = image.dimensions();
  if (size.isEmpty()) {
    HideCursor(serial);
    return;
  }

  gfx::Size image_size = gfx::SkISizeToSize(size);
  if (image_size != size_) {
    wl_buffer* buffer =
        wl::CreateSHMBuffer(image_size, shared_memory_.get(), shm_);
    if (!buffer) {
      LOG(ERROR) << "Failed to create SHM buffer for Cursor Bitmap.";
      wl_pointer_set_cursor(input_pointer_, serial, nullptr, 0, 0);
      return;
    }
    buffer_.reset(buffer);
    size_ = image_size;
  }
  wl::DrawBitmapToSHMB(size_, *shared_memory_, image);

  wl_pointer_set_cursor(input_pointer_, serial, pointer_surface_.get(),
                        location.x(), location.y());
  wl_surface_attach(pointer_surface_.get(), buffer_.get(), 0, 0);
  wl_surface_damage(pointer_surface_.get(), 0, 0, size_.width(),
                    size_.height());
  wl_surface_commit(pointer_surface_.get());
}

void WaylandCursor::HideCursor(uint32_t serial) {
  size_ = gfx::Size();
  wl_pointer_set_cursor(input_pointer_, serial, nullptr, 0, 0);

  buffer_.reset();

  if (shared_memory_->handle().GetHandle()) {
    shared_memory_->Unmap();
    shared_memory_->Close();
  }
}

}  // namespace ui
