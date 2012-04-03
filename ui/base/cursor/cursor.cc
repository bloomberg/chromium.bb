// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

namespace ui {

Cursor::Cursor()
    : native_type_(0),
      platform_cursor_(0) {
}

Cursor::Cursor(int type)
    : native_type_(type),
      platform_cursor_(0) {
}

Cursor::Cursor(const Cursor& cursor)
    : native_type_(cursor.native_type_),
      platform_cursor_(cursor.platform_cursor_) {
  if (native_type_ == kCursorCustom)
    RefCustomCursor();
}

Cursor::~Cursor() {
  if (native_type_ == kCursorCustom)
    UnrefCustomCursor();
}

void Cursor::SetPlatformCursor(const PlatformCursor& platform) {
  if (platform_cursor_)
    UnrefCustomCursor();
  native_type_ = kCursorCustom;
  platform_cursor_ = platform;
  RefCustomCursor();
}


}  // namespace ui
