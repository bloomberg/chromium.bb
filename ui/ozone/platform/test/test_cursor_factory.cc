// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/test/test_cursor_factory.h"

namespace ui {

TestCursorFactory::TestCursorFactory()
    : cursor_window_(gfx::kNullAcceleratedWidget) {}

TestCursorFactory::~TestCursorFactory() {}

gfx::AcceleratedWidget TestCursorFactory::GetCursorWindow() {
  return cursor_window_;
}

void TestCursorFactory::SetBitmapCursor(
    gfx::AcceleratedWidget widget,
    scoped_refptr<BitmapCursorOzone> cursor) {
  cursor_window_ = widget;
  cursor_ = cursor;
}

}  // namespace ui
