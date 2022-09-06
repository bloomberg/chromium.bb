// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include <algorithm>

#include "build/build_config.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace content {

WebCursor::WebCursor() = default;

WebCursor::~WebCursor() = default;

WebCursor::WebCursor(const ui::Cursor& cursor) {
  SetCursor(cursor);
}

WebCursor::WebCursor(const WebCursor& other) = default;

bool WebCursor::SetCursor(const ui::Cursor& cursor) {
  // This value is just large enough to accommodate:
  // - kMaximumCursorSize in Blink's EventHandler
  // - kCursorSize in Chrome's DevToolsEyeDropper
  static constexpr int kMaximumCursorSize = 150;
  // This value limits the underlying bitmap to a reasonable size.
  static constexpr int kMaximumBitmapSize = 1024;
  if (cursor.image_scale_factor() < 0.01f ||
      cursor.image_scale_factor() > 100.f ||
      (cursor.type() == ui::mojom::CursorType::kCustom &&
       (cursor.custom_bitmap().width() > kMaximumBitmapSize ||
        cursor.custom_bitmap().height() > kMaximumBitmapSize ||
        cursor.custom_bitmap().width() / cursor.image_scale_factor() >
            kMaximumCursorSize ||
        cursor.custom_bitmap().height() / cursor.image_scale_factor() >
            kMaximumCursorSize))) {
    return false;
  }

  CleanupPlatformData();
  cursor_ = cursor;

  // Clamp the hotspot to the custom image's dimensions.
  if (cursor_.type() == ui::mojom::CursorType::kCustom) {
    cursor_.set_custom_hotspot(
        gfx::Point(std::max(0, std::min(cursor_.custom_bitmap().width() - 1,
                                        cursor_.custom_hotspot().x())),
                   std::max(0, std::min(cursor_.custom_bitmap().height() - 1,
                                        cursor_.custom_hotspot().y()))));
  }

  return true;
}

bool WebCursor::operator==(const WebCursor& other) const {
  return
#if defined(USE_AURA) || defined(USE_OZONE)
      rotation_ == other.rotation_ &&
#endif
      cursor_ == other.cursor_;
}

bool WebCursor::operator!=(const WebCursor& other) const {
  return !(*this == other);
}

}  // namespace content
