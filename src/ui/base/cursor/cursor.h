// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_H_
#define UI_BASE_CURSOR_CURSOR_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

// Ref-counted cursor that supports both default and custom cursors.
class COMPONENT_EXPORT(UI_BASE_CURSOR) Cursor {
 public:
  Cursor();
  Cursor(mojom::CursorType type);
  Cursor(const Cursor& cursor);
  ~Cursor();

  void SetPlatformCursor(scoped_refptr<PlatformCursor> platform_cursor);

  mojom::CursorType type() const { return type_; }
  scoped_refptr<PlatformCursor> platform() const { return platform_cursor_; }
  float image_scale_factor() const { return image_scale_factor_; }
  void set_image_scale_factor(float scale) { image_scale_factor_ = scale; }

  const SkBitmap& custom_bitmap() const { return custom_bitmap_; }
  void set_custom_bitmap(const SkBitmap& bitmap) { custom_bitmap_ = bitmap; }

  const gfx::Point& custom_hotspot() const { return custom_hotspot_; }
  void set_custom_hotspot(const gfx::Point& hotspot) {
    custom_hotspot_ = hotspot;
  }

  // Note: custom cursor comparison may perform expensive pixel equality checks!
  bool operator==(const Cursor& cursor) const;
  bool operator!=(const Cursor& cursor) const { return !(*this == cursor); }

  bool operator==(mojom::CursorType type) const { return type_ == type; }
  bool operator!=(mojom::CursorType type) const { return type_ != type; }

 private:
  // The basic cursor type.
  mojom::CursorType type_ = mojom::CursorType::kNull;

  // The native platform cursor.
  scoped_refptr<PlatformCursor> platform_cursor_;

  // The scale factor for the cursor bitmap.
  float image_scale_factor_ = 1.0f;

  // The hotspot for the cursor. This is only used for the custom cursor type.
  gfx::Point custom_hotspot_;

  // The bitmap for the cursor. This is only used for the custom cursor type.
  SkBitmap custom_bitmap_;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_H_
