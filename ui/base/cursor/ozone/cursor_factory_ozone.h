// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_OZONE_CURSOR_FACTORY_OZONE_H_
#define UI_BASE_CURSOR_OZONE_CURSOR_FACTORY_OZONE_H_

#include "ui/base/cursor/cursor.h"
#include "ui/base/ui_base_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

namespace ui {

class UI_BASE_EXPORT CursorFactoryOzone {
 public:
  CursorFactoryOzone();
  virtual ~CursorFactoryOzone();

  // Returns the static instance.
  static CursorFactoryOzone* GetInstance();

  // Sets the static instance. Ownership is retained by the caller.
  static void SetInstance(CursorFactoryOzone* impl);

  // Return the default cursor of the specified type. The types are listed in
  // ui/base/cursor/cursor.h. Default cursors are managed by the implementation
  // and must live indefinitely; there's no way to know when to free them.
  PlatformCursor GetDefaultCursor(int type);

  // Return a image cursor from the specified image & hotspot. Image cursors
  // are referenced counted and have an initial refcount of 1. Therefore, each
  // CreateImageCursor call must be matched with a call to UnrefImageCursor.
  PlatformCursor CreateImageCursor(const SkBitmap& bitmap,
                                   const gfx::Point& hotspot);

  // Increment platform image cursor refcount.
  void RefImageCursor(PlatformCursor cursor);

  // Decrement platform image cursor refcount.
  void UnrefImageCursor(PlatformCursor cursor);

  // Change the active cursor for an AcceleratedWidget.
  // TODO(spang): Move this.
  void SetCursor(gfx::AcceleratedWidget widget, PlatformCursor cursor);

  // Warp the cursor within an AccelerateWidget.
  // TODO(spang): Move this.
  void MoveCursorTo(gfx::AcceleratedWidget widget, const gfx::Point& location);

 private:
  static CursorFactoryOzone* impl_;  // not owned
};

}  // namespace gfx

#endif  // UI_BASE_CURSOR_OZONE_CURSOR_FACTORY_OZONE_H_
