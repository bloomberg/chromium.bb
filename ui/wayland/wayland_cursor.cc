// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wayland/wayland_cursor.h"

#include <cairo.h>

#include "base/logging.h"
#include "ui/wayland/wayland_display.h"
#include "ui/wayland/wayland_shm_buffer.h"

namespace {

struct PointerImage {
  const char* filename;
  int hotspot_x, hotspot_y;
};

// TODO(dnicoara) Add more pointer images and fix the path.
const PointerImage kPointerImages[] = {
  {"left_ptr.png",        10, 5},
  {"hand.png",            10, 5},
};

}  // namespace

namespace ui {

WaylandCursor::WaylandCursor(WaylandDisplay* display)
    : display_(display),
      buffer_(NULL) {
}

WaylandCursor::~WaylandCursor() {
  if (buffer_) {
    delete buffer_;
    buffer_ = NULL;
  }
}

void WaylandCursor::ChangeCursor(Type type) {
  const PointerImage& ptr = kPointerImages[type];

  cairo_surface_t* ptr_surface = cairo_image_surface_create_from_png(
      ptr.filename);

  if (!ptr_surface) {
    LOG(ERROR) << "Failed to create cursor surface";
    return;
  }

  int width = cairo_image_surface_get_width(ptr_surface);
  int height = cairo_image_surface_get_height(ptr_surface);

  if (buffer_) {
    delete buffer_;
    buffer_ = NULL;
  }

  // TODO(dnicoara) There should be a simpler way to create a wl_buffer for
  // a cairo surface. Meantime we just copy the contents of the cairo surface
  // created from file to the cairo surface created using a shm file.
  buffer_ = new WaylandShmBuffer(display_, width, height);
  cairo_surface_t* shared_surface = buffer_->data_surface();

  cairo_t* cr = cairo_create(shared_surface);
  cairo_set_source_surface(cr, ptr_surface, 0, 0);
  cairo_rectangle(cr, 0, 0, width, height);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_fill(cr);

  display_->SetCursor(buffer_, ptr.hotspot_x, ptr.hotspot_y);
}

}  // namespace ui
