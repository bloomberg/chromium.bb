// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WAYLAND_WAYLAND_SHM_BUFFER_H_
#define UI_WAYLAND_WAYLAND_SHM_BUFFER_H_

#include <stdint.h>

#include "base/basictypes.h"
#include "ui/wayland/wayland_buffer.h"

typedef struct _cairo_surface cairo_surface_t;

namespace ui {

class WaylandDisplay;

// A SHM implementation for the WaylandBuffer.
class WaylandShmBuffer : public WaylandBuffer {
 public:
  // Creates a Wayland buffer with the given width and height.
  WaylandShmBuffer(WaylandDisplay* display, uint32_t width, uint32_t height);
  virtual ~WaylandShmBuffer();

  // Returns a pointer to the surface associated with the buffer.
  // This object maintains ownership of the surface.
  cairo_surface_t* data_surface() const { return data_surface_; }

 private:
  // The surface associated with the Wayland buffer.
  cairo_surface_t* data_surface_;

  DISALLOW_COPY_AND_ASSIGN(WaylandShmBuffer);
};

}  // namespace ui

#endif  // UI_WAYLAND_WAYLAND_SHM_BUFFER_H_
