// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CAPTURER_MOUSE_CURSOR_SHAPE_H_
#define REMOTING_CAPTURER_MOUSE_CURSOR_SHAPE_H_

#include <string>

#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSize.h"

namespace remoting {

// Type used to return mouse cursor shape from video capturers.
struct MouseCursorShape {
  MouseCursorShape();
  ~MouseCursorShape();

  // Size of the cursor in screen pixels.
  SkISize size;

  // Coordinates of the cursor hotspot relative to upper-left corner.
  SkIPoint hotspot;

  // Cursor pixmap data in 32-bit BGRA format.
  std::string data;
};

}  // namespace remoting

#endif  // REMOTING_CAPTURER_MOUSE_CURSOR_SHAPE_H_
