// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WAYLAND_WAYLAND_CURSOR_H_
#define UI_WAYLAND_WAYLAND_CURSOR_H_

#include "base/basictypes.h"

namespace ui {

class WaylandDisplay;
class WaylandShmBuffer;

// This class allows applications to change the currently displaying cursor.
class WaylandCursor {
 public:
  // Types of Wayland cursors supported
  enum Type {
    ARROW_CURSOR,
    HAND_CURSOR,
  };

  explicit WaylandCursor(WaylandDisplay* display);
  ~WaylandCursor();

  // Used to change the current type to the type specified in 'type'
  void ChangeCursor(Type type);

 private:
  // Pointer to the current display. This is not owned by this class.
  WaylandDisplay* display_;
  // The currently set cursor image.
  // This class should dispose of this on deletion.
  WaylandShmBuffer* buffer_;

  DISALLOW_COPY_AND_ASSIGN(WaylandCursor);
};

}  // namespace ui

#endif  // UI_WAYLAND_WAYLAND_CURSOR_H_
