// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/capturer/mouse_cursor_shape.h"

namespace remoting {

MouseCursorShape::MouseCursorShape()
  : size(SkISize::Make(0, 0)),
    hotspot(SkIPoint::Make(0, 0)) {
}

MouseCursorShape::~MouseCursorShape() {
}

}  // namespace remoting
