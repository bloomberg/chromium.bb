// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_CONSOLE_BUFFER_H_
#define UI_OZONE_PLATFORM_DRI_DRI_CONSOLE_BUFFER_H_

#include "base/macros.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkSurface.h"

class SkCanvas;

namespace ui {

class DriWrapper;

// Wrapper for the console buffer. This is the buffer that is allocated by
// default by the system and is used when no application is controlling the
// CRTC. Keeps track of the native properties of the buffer and wraps the pixel
// memory into a SkSurface which can be used to draw into using Skia.
class DriConsoleBuffer {
 public:
  DriConsoleBuffer(DriWrapper* dri, uint32_t framebuffer);
  ~DriConsoleBuffer();

  SkCanvas* canvas() { return surface_->getCanvas(); }

  // Memory map the backing pixels and wrap them in |surface_|.
  bool Initialize();

 protected:
  DriWrapper* dri_;  // Not owned.

  // Wrapper around the native pixel memory.
  skia::RefPtr<SkSurface> surface_;

  // Length of a row of pixels.
  uint32_t stride_;

  // Buffer handle used by the DRM allocator.
  uint32_t handle_;

  // Buffer ID used by the DRM modesettings API.
  uint32_t framebuffer_;

  // Memory map base address.
  void* mmap_base_;

  // Memory map size.
  size_t mmap_size_;

  DISALLOW_COPY_AND_ASSIGN(DriConsoleBuffer);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_CONSOLE_BUFFER_H_
