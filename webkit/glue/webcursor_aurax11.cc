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
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/size_conversions.h"

const ui::PlatformCursor WebCursor::GetPlatformCursor() {
  if (platform_cursor_)
    return platform_cursor_;

  if (custom_data_.size() == 0)
    return 0;

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                   custom_size_.width(), custom_size_.height());
  bitmap.allocPixels();
  memcpy(bitmap.getAddr32(0, 0), custom_data_.data(), custom_data_.size());

  gfx::Point hotspot = hotspot_;
  if (device_scale_factor_ != custom_scale_) {
    float scale = device_scale_factor_ / custom_scale_;
    gfx::Size scaled_size =
        gfx::ToFlooredSize(gfx::ScaleSize(custom_size_, scale));
    bitmap = skia::ImageOperations::Resize(bitmap,
        skia::ImageOperations::RESIZE_BETTER,
        scaled_size.width(),
        scaled_size.height());
    hotspot = gfx::ToFlooredPoint(gfx::ScalePoint(hotspot, scale));
  }

  XcursorImage* image = ui::SkBitmapToXcursorImage(&bitmap, hotspot);
  platform_cursor_ = ui::CreateReffedCustomXCursor(image);
  return platform_cursor_;
}

void WebCursor::SetDeviceScaleFactor(float scale_factor) {
  if (device_scale_factor_ == scale_factor)
    return;

  device_scale_factor_ = scale_factor;
  if (platform_cursor_)
    ui::UnrefCustomXCursor(platform_cursor_);
  platform_cursor_ = 0;
  // It is not necessary to recreate platform_cursor_ yet, since it will be
  // recreated on demand when GetPlatformCursor is called.
}

void WebCursor::InitPlatformData() {
  platform_cursor_ = 0;
  device_scale_factor_ = 1.f;
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

  device_scale_factor_ = other.device_scale_factor_;
}
