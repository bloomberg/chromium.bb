// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webcursor.h"

#include <X11/Xcursor/Xcursor.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>

#include "base/logging.h"
#include "skia/ext/image_operations.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/x/x11_util.h"

const ui::PlatformCursor WebCursor::GetPlatformCursor() {
  if (platform_cursor_)
    return platform_cursor_;

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                   custom_size_.width(), custom_size_.height());
  bitmap.allocPixels();
  memcpy(bitmap.getAddr32(0, 0), custom_data_.data(), custom_data_.size());

  XcursorImage* image = NULL;
  if (scale_factor_ == 1.f) {
    image = ui::SkBitmapToXcursorImage(&bitmap, hotspot_);
  } else {
    gfx::Size scaled_size = custom_size_.Scale(scale_factor_);
    SkBitmap scaled_bitmap = skia::ImageOperations::Resize(bitmap,
        skia::ImageOperations::RESIZE_BETTER,
        scaled_size.width(),
        scaled_size.height());
    image = ui::SkBitmapToXcursorImage(&scaled_bitmap,
                                       hotspot_.Scale(scale_factor_));
  }
  platform_cursor_ = ui::CreateReffedCustomXCursor(image);
  return platform_cursor_;
}

void WebCursor::SetScaleFactor(float scale_factor) {
  if (scale_factor_ == scale_factor)
    return;

  scale_factor_ = scale_factor;
  if (platform_cursor_)
    ui::UnrefCustomXCursor(platform_cursor_);
  platform_cursor_ = 0;
  // It is not necessary to recreate platform_cursor_ yet, since it will be
  // recreated on demand when GetPlatformCursor is called.
}

void WebCursor::InitPlatformData() {
  platform_cursor_ = 0;
  scale_factor_ = 1.f;
}

bool WebCursor::SerializePlatformData(Pickle* pickle) const {
  return true;
}

bool WebCursor::DeserializePlatformData(PickleIterator* iter) {
  return true;
}

bool WebCursor::IsPlatformDataEqual(const WebCursor& other) const {
  return true;
}

void WebCursor::CleanupPlatformData() {
  if (platform_cursor_) {
    ui::UnrefCustomXCursor(platform_cursor_);
    platform_cursor_ = 0;
  }
}

void WebCursor::CopyPlatformData(const WebCursor& other) {
  if (platform_cursor_)
    ui::UnrefCustomXCursor(platform_cursor_);
  platform_cursor_ = other.platform_cursor_;
  if (platform_cursor_)
    ui::RefCustomXCursor(platform_cursor_);

  scale_factor_ = other.scale_factor_;
}
