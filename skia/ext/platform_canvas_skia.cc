// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/platform_canvas.h"

#include "base/debug/trace_event.h"
#include "skia/ext/bitmap_platform_device.h"

// TODO(reveman): a lot of unnecessary duplication of code from
// platform_canvas_[win|linux|mac].cc in here. Need to refactor
// PlatformCanvas to avoid this:
// http://code.google.com/p/chromium/issues/detail?id=119555

namespace skia {

PlatformCanvas::PlatformCanvas(int width, int height, bool is_opaque) {
  TRACE_EVENT2("skia", "PlatformCanvas::PlatformCanvas",
               "width", width, "height", height);
  if (!initialize(width, height, is_opaque))
    SK_CRASH();
}

#if defined(WIN32)
PlatformCanvas::PlatformCanvas(int width,
                               int height,
                               bool is_opaque,
                               HANDLE shared_section) {
  TRACE_EVENT2("skia", "PlatformCanvas::PlatformCanvas",
               "width", width, "height", height);
  if (!initialize(width, height, is_opaque, shared_section))
    SK_CRASH();
}
#elif defined(__APPLE__)
PlatformCanvas::PlatformCanvas(int width, int height, bool is_opaque,
                               uint8_t* data) {
  TRACE_EVENT2("skia", "PlatformCanvas::PlatformCanvas",
               "width", width, "height", height);
  if (!initialize(width, height, is_opaque, data))
    SK_CRASH();
}
PlatformCanvas::PlatformCanvas(int width,
                               int height,
                               bool is_opaque,
                               CGContextRef context) {
  TRACE_EVENT2("skia", "PlatformCanvas::PlatformCanvas",
               "width", width, "height", height);
  if (!initialize(context, width, height, is_opaque))
    SK_CRASH();
}
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
      defined(__sun) || defined(ANDROID)
PlatformCanvas::PlatformCanvas(int width, int height, bool is_opaque,
                               uint8_t* data) {
  TRACE_EVENT2("skia", "PlatformCanvas::PlatformCanvas",
               "width", width, "height", height);
  if (!initialize(width, height, is_opaque, data))
    SK_CRASH();
}
#endif

PlatformCanvas::~PlatformCanvas() {
}

#if defined(WIN32)
bool PlatformCanvas::initialize(int width,
                                int height,
                                bool is_opaque,
                                HANDLE shared_section) {
  // Use platform specific device for shared_section.
  if (shared_section)
    return initializeWithDevice(BitmapPlatformDevice::Create(
        width, height, is_opaque, shared_section));

  return initializeWithDevice(new SkDevice(
      SkBitmap::kARGB_8888_Config, width, height, is_opaque));
}
#elif defined(__APPLE__)
bool PlatformCanvas::initialize(int width,
                                int height,
                                bool is_opaque,
                                uint8_t* data) {
  // Use platform specific device for data.
  if (data)
    return initializeWithDevice(BitmapPlatformDevice::CreateWithData(
        data, width, height, is_opaque));

  return initializeWithDevice(new SkDevice(
      SkBitmap::kARGB_8888_Config, width, height, is_opaque));
}

bool PlatformCanvas::initialize(CGContextRef context,
                                int width,
                                int height,
                                bool is_opaque) {
  return initializeWithDevice(BitmapPlatformDevice::Create(
      context, width, height, is_opaque));
}
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
      defined(__sun) || defined(ANDROID)
bool PlatformCanvas::initialize(int width, int height, bool is_opaque,
                                uint8_t* data) {
  // Use platform specific device for data.
  if (data)
    return initializeWithDevice(BitmapPlatformDevice::Create(
        width, height, is_opaque, data));

  return initializeWithDevice(new SkDevice(
      SkBitmap::kARGB_8888_Config, width, height, is_opaque));
}
#endif

}  // namespace skia
