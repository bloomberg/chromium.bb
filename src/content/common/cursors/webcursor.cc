// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include <algorithm>

#include "build/build_config.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace content {

WebCursor::WebCursor() = default;

WebCursor::~WebCursor() {
  CleanupPlatformData();
}

WebCursor::WebCursor(const ui::Cursor& cursor) {
  SetCursor(cursor);
}

WebCursor::WebCursor(const WebCursor& other) {
  CopyAllData(other);
}

WebCursor& WebCursor::operator=(const WebCursor& other) {
  CleanupPlatformData();
  CopyAllData(other);
  return *this;
}

bool WebCursor::SetCursor(const ui::Cursor& cursor) {
  static constexpr int kMaxSize = 1024;
  if (cursor.image_scale_factor() < 0.01f ||
      cursor.image_scale_factor() > 100.f ||
      (cursor.type() == ui::mojom::CursorType::kCustom &&
       (cursor.custom_bitmap().width() > kMaxSize ||
        cursor.custom_bitmap().height() > kMaxSize ||
        cursor.custom_bitmap().width() / cursor.image_scale_factor() >
            kMaxSize ||
        cursor.custom_bitmap().height() / cursor.image_scale_factor() >
            kMaxSize))) {
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
  return cursor_ == other.cursor_ &&
#if defined(USE_AURA) || defined(USE_OZONE)
         rotation_ == other.rotation_ &&
#endif
         IsPlatformDataEqual(other);
}

bool WebCursor::operator!=(const WebCursor& other) const {
  return !(*this == other);
}

void WebCursor::CopyAllData(const WebCursor& other) {
  SetCursor(other.cursor_);
  CopyPlatformData(other);
}

}  // namespace content
