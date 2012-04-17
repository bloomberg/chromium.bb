// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <psapi.h>

#include "base/debug/trace_event.h"
#include "skia/ext/bitmap_platform_device_win.h"
#include "skia/ext/platform_canvas.h"

namespace skia {

PlatformCanvas::PlatformCanvas(int width, int height, bool is_opaque) {
  TRACE_EVENT2("skia", "PlatformCanvas::PlatformCanvas",
               "width", width, "height", height);
  initialize(width, height, is_opaque, NULL);
}

PlatformCanvas::PlatformCanvas(int width,
                               int height,
                               bool is_opaque,
                               HANDLE shared_section) {
  TRACE_EVENT2("skia", "PlatformCanvas::PlatformCanvas",
               "width", width, "height", height);
  initialize(width, height, is_opaque, shared_section);
}

PlatformCanvas::~PlatformCanvas() {
}

bool PlatformCanvas::initialize(int width,
                               int height,
                               bool is_opaque,
                               HANDLE shared_section) {
  return initializeWithDevice(BitmapPlatformDevice::Create(
      width, height, is_opaque, shared_section));
}

}  // namespace skia
