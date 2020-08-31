// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cursors/webcursor.h"

#include <algorithm>

#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_lookup.h"
#include "ui/base/cursor/cursor_util.h"
#include "ui/ozone/public/cursor_factory_ozone.h"

namespace content {

ui::PlatformCursor WebCursor::GetPlatformCursor(const ui::Cursor& cursor) {
  if (!platform_cursor_) {
    platform_cursor_ = ui::CursorFactoryOzone::GetInstance()->CreateImageCursor(
        GetCursorBitmap(cursor), GetCursorHotspot(cursor),
        cursor.image_scale_factor());
  }

  return platform_cursor_;
}

void WebCursor::SetDisplayInfo(const display::Display& display) {
  if (rotation_ == display.panel_rotation() &&
      device_scale_factor_ == display.device_scale_factor() &&
      maximum_cursor_size_ == display.maximum_cursor_size())
    return;
  device_scale_factor_ = display.device_scale_factor();
  // The cursor should use the panel's physical rotation instead of
  // rotation. They can be different on ChromeOS but the same on
  // other platforms.
  rotation_ = display.panel_rotation();
  maximum_cursor_size_ = display.maximum_cursor_size();
  // TODO(oshima): Identify if it's possible to remove this check here and move
  // the kDefaultMaxSize constants to a single place. crbug.com/603512
  if (maximum_cursor_size_.width() == 0 || maximum_cursor_size_.height() == 0)
    maximum_cursor_size_ = gfx::Size(kDefaultMaxSize, kDefaultMaxSize);
  CleanupPlatformData();
  // It is not necessary to recreate platform_cursor_ yet, since it will be
  // recreated on demand when GetPlatformCursor is called.
}

float WebCursor::GetCursorScaleFactor(SkBitmap* bitmap) {
  DCHECK_LT(0, maximum_cursor_size_.width());
  DCHECK_LT(0, maximum_cursor_size_.height());
  return std::min(
      {device_scale_factor_ / cursor_.image_scale_factor(),
       static_cast<float>(maximum_cursor_size_.width()) / bitmap->width(),
       static_cast<float>(maximum_cursor_size_.height()) / bitmap->height()});
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {
  if (platform_cursor_) {
    ui::CursorFactoryOzone::GetInstance()->UnrefImageCursor(platform_cursor_);
    platform_cursor_ = NULL;
  }
  custom_cursor_.reset();
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
  if (platform_cursor_)
    ui::CursorFactoryOzone::GetInstance()->UnrefImageCursor(platform_cursor_);
  platform_cursor_ = other.platform_cursor_;
  if (platform_cursor_)
    ui::CursorFactoryOzone::GetInstance()->RefImageCursor(platform_cursor_);

  device_scale_factor_ = other.device_scale_factor_;
  maximum_cursor_size_ = other.maximum_cursor_size_;
}

}  // namespace content
