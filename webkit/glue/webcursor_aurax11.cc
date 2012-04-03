// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webcursor.h"

#include <X11/Xcursor/Xcursor.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/x/x11_util.h"

const ui::PlatformCursor WebCursor::GetPlatformCursor() {
  if (platform_cursor_)
    return platform_cursor_;

  XcursorImage* image =
      XcursorImageCreate(custom_size_.width(), custom_size_.height());
  image->xhot = hotspot_.x();
  image->yhot = hotspot_.y();
  uint32* pixels = image->pixels;

  if (custom_size_.width() && custom_size_.height()) {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                     custom_size_.width(), custom_size_.height());
    bitmap.allocPixels();
    memcpy(bitmap.getAddr32(0, 0), custom_data_.data(), custom_data_.size());

    bitmap.lockPixels();
    int height = bitmap.height(), width = bitmap.width();
    for (int y = 0, i = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        uint32 pixel = bitmap.getAddr32(0, y)[x];
        int alpha = SkColorGetA(pixel);
        if (alpha != 0 && alpha != 255)
          pixels[i] = SkUnPreMultiply::PMColorToColor(pixel);
        else
          pixels[i] = pixel;
        ++i;
      }
    }
    bitmap.unlockPixels();
  }

  platform_cursor_ = ui::CreateReffedCustomXCursor(image);
  return platform_cursor_;
}

void WebCursor::InitPlatformData() {
  platform_cursor_ = 0;
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
}
