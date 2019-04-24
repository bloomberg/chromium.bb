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

WaylandCursor::WaylandCursor() = default;

WaylandCursor::~WaylandCursor() = default;

// static
void WaylandCursor::OnBufferRelease(void* data, wl_buffer* buffer) {
  auto* cursor = static_cast<WaylandCursor*>(data);
  DCHECK(cursor->buffers_.count(buffer) > 0);
  cursor->buffers_.erase(buffer);
}

void WaylandCursor::Init(wl_pointer* pointer, WaylandConnection* connection) {
  DCHECK(connection);

  input_pointer_ = pointer;

  shm_ = connection->shm();
  pointer_surface_.reset(
      wl_compositor_create_surface(connection->compositor()));
}

void WaylandCursor::UpdateBitmap(const std::vector<SkBitmap>& cursor_image,
                                 const gfx::Point& hotspot,
                                 uint32_t serial) {
  if (!input_pointer_)
    return;

  if (!cursor_image.size())
    return HideCursor(serial);

  const SkBitmap& image = cursor_image[0];
  SkISize size = image.dimensions();
  if (size.isEmpty())
    return HideCursor(serial);

  gfx::Size image_size = gfx::SkISizeToSize(size);
  auto shared_memory = std::make_unique<base::SharedMemory>();
  wl::Object<wl_buffer> buffer(
      wl::CreateSHMBuffer(image_size, shared_memory.get(), shm_));
  if (!buffer) {
    LOG(ERROR) << "Failed to create SHM buffer for Cursor Bitmap.";
    return HideCursor(serial);
  }

  static const struct wl_buffer_listener wl_buffer_listener {
    &WaylandCursor::OnBufferRelease
  };
  wl_buffer_add_listener(buffer.get(), &wl_buffer_listener, this);

  wl::DrawBitmapToSHMB(image_size, *shared_memory, image);

  wl_pointer_set_cursor(input_pointer_, serial, pointer_surface_.get(),
                        hotspot.x(), hotspot.y());
  wl_surface_attach(pointer_surface_.get(), buffer.get(), 0, 0);
  wl_surface_commit(pointer_surface_.get());

  buffers_.emplace(
      buffer.get(),
      std::pair<wl::Object<wl_buffer>, std::unique_ptr<base::SharedMemory>>(
          std::move(buffer), std::move(shared_memory)));
}

void WaylandCursor::HideCursor(uint32_t serial) {
  DCHECK(input_pointer_);
  wl_pointer_set_cursor(input_pointer_, serial, nullptr, 0, 0);
}

}  // namespace ui
