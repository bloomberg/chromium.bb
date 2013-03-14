// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_LOADER_H_
#define UI_BASE_CURSOR_CURSOR_LOADER_H_

#include "base/logging.h"
#include "ui/base/ui_export.h"
#include "ui/gfx/display.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

namespace ui {

class UI_EXPORT CursorLoader {
 public:
  CursorLoader() {}
  virtual ~CursorLoader() {}

  // Returns the display the loader loads images for.
  const gfx::Display& display() const {
    return display_;
  }

  // Sets the display the loader loads images for.
  void set_display(const gfx::Display& display) {
    display_ = display;
  }

  // Creates a cursor from an image resource and puts it in the cursor map.
  virtual void LoadImageCursor(int id,
                               int resource_id,
                               const gfx::Point& hot) = 0;

  // Creates an animated cursor from an image resource and puts it in the
  // cursor map. The image is assumed to be a concatenation of animation frames
  // from left to right. Also, each frame is assumed to be square
  // (width == height).
  // |frame_delay_ms| is the delay between frames in millisecond.
  virtual void LoadAnimatedCursor(int id,
                                  int resource_id,
                                  const gfx::Point& hot,
                                  int frame_delay_ms) = 0;

  // Unloads all the cursors.
  virtual void UnloadAll() = 0;

  // Sets the platform cursor based on the native type of |cursor|.
  virtual void SetPlatformCursor(gfx::NativeCursor* cursor) = 0;

  // Creates a CursorLoader.
  static CursorLoader* Create();

 private:
  // The display the loader loads images for.
  gfx::Display display_;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOADER_H_
