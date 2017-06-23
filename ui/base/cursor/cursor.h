// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_H_
#define UI_BASE_CURSOR_CURSOR_H_

#include "build/build_config.h"
#include "ui/base/cursor/cursor_type.h"
#include "ui/base/ui_base_export.h"

#if defined(OS_WIN)
typedef struct HINSTANCE__* HINSTANCE;
typedef struct HICON__* HICON;
typedef HICON HCURSOR;
#endif

namespace ui {

#if defined(OS_WIN)
typedef ::HCURSOR PlatformCursor;
#elif defined(USE_X11)
typedef unsigned long PlatformCursor;
#else
typedef void* PlatformCursor;
#endif

// Ref-counted cursor that supports both default and custom cursors.
class UI_BASE_EXPORT Cursor {
 public:
  Cursor();

  // Implicit constructor.
  Cursor(CursorType type);

  // Allow copy.
  Cursor(const Cursor& cursor);

  ~Cursor();

  void SetPlatformCursor(const PlatformCursor& platform);

  void RefCustomCursor();
  void UnrefCustomCursor();

  CursorType native_type() const { return native_type_; }
  PlatformCursor platform() const { return platform_cursor_; }
  float device_scale_factor() const {
    return device_scale_factor_;
  }
  void set_device_scale_factor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }

  bool operator==(CursorType type) const { return native_type_ == type; }
  bool operator==(const Cursor& cursor) const {
    return native_type_ == cursor.native_type_ &&
           platform_cursor_ == cursor.platform_cursor_ &&
           device_scale_factor_ == cursor.device_scale_factor_;
  }
  bool operator!=(CursorType type) const { return native_type_ != type; }
  bool operator!=(const Cursor& cursor) const {
    return native_type_ != cursor.native_type_ ||
           platform_cursor_ != cursor.platform_cursor_ ||
           device_scale_factor_ != cursor.device_scale_factor_;
  }

  void operator=(const Cursor& cursor) {
    Assign(cursor);
  }

 private:
  void Assign(const Cursor& cursor);

  // See definitions above.
  CursorType native_type_;

  PlatformCursor platform_cursor_;

  // The device scale factor for the cursor.
  float device_scale_factor_;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_H_
