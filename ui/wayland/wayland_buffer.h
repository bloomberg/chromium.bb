// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WAYLAND_WAYLAND_BUFFER_H_
#define UI_WAYLAND_WAYLAND_BUFFER_H_

#include "base/basictypes.h"

struct wl_buffer;

namespace ui {

// Wrapper around a wl_buffer. A wl_buffer is associated with a drawing
// surface.
//
// This class is a basic interface of what a buffer should be/provide.
// Implementations whould create and destroy the wl_buffer.
class WaylandBuffer {
 public:
  WaylandBuffer();
  virtual ~WaylandBuffer();

  wl_buffer* buffer() const { return buffer_; }

 protected:
  // Owned by this object and should be destroyed by this object.
  wl_buffer* buffer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandBuffer);
};

}  // namespace ui

#endif  // UI_WAYLAND_WAYLAND_BUFFER_H_
