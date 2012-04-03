// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

#include "ui/base/x/x11_util.h"

namespace ui {

void Cursor::RefCustomCursor() {
  ui::RefCustomXCursor(platform_cursor_);
}

void Cursor::UnrefCustomCursor() {
  ui::UnrefCustomXCursor(platform_cursor_);
}

}  // namespace ui
