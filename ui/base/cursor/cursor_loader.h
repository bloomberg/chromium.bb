// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_LOADER_H_
#define UI_BASE_CURSOR_CURSOR_LOADER_H_

#include "base/logging.h"
#include "base/strings/string16.h"
#include "ui/base/ui_export.h"
#include "ui/gfx/display.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

namespace ui {

class UI_EXPORT CursorLoader {
 public:
  CursorLoader() : scale_(1.f) {}
  virtual ~CursorLoader() {}

  // Returns the display the loader loads images for.
  const gfx::Display& display() const {
    return display_;
  }

  // Sets the display the loader loads images for.
  void set_display(const gfx::Display& display) {
    display_ = display;
  }

  // Returns the current scale of the mouse cursor icon.
  float scale() const {
    return scale_;
  }

  // Sets the scale of the mouse cursor icon.
  void set_scale(const float scale) {
    scale_ = scale;
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

  // The current scale of the mouse cursor icon.
  float scale_;

  DISALLOW_COPY_AND_ASSIGN(CursorLoader);
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOADER_H_
