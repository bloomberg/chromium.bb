// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OZONE_IMPL_DRM_SKBITMAP_OZONE_H_
#define UI_GFX_OZONE_IMPL_DRM_SKBITMAP_OZONE_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {

// Extend the SkBitmap interface to keep track of additional parameters used by
// the DRM stack when allocating buffers.
class DrmSkBitmapOzone : public SkBitmap {
 public:
  DrmSkBitmapOzone(int fd);
  virtual ~DrmSkBitmapOzone();

  // Allocates the backing pixels using DRM.
  // Return true on success, false otherwise.
  virtual bool Initialize();

  uint32_t get_handle() const { return handle_; };

  uint32_t get_framebuffer() const { return framebuffer_; };

  int get_fd() const { return fd_; };

  // Return the color depth of a pixel in this buffer.
  uint8_t GetColorDepth() const;

 private:
  friend class DrmAllocator;
  friend class HardwareDisplayControllerOzone;

  void set_handle(uint32_t handle) { handle_ = handle; };
  void set_framebuffer(uint32_t framebuffer) { framebuffer_ = framebuffer; };

  // File descriptor used by the DRM allocator to request buffers from the DRM
  // stack.
  int fd_;

  // Buffer handle used by the DRM allocator.
  uint32_t handle_;

  // Buffer ID used by the DRM modesettings API. This is set when the buffer is
  // registered with the CRTC.
  uint32_t framebuffer_;

  DISALLOW_COPY_AND_ASSIGN(DrmSkBitmapOzone);
};

}  // namespace gfx

#endif  // UI_GFX_OZONE_IMPL_DRM_SKBITMAP_OZONE_H_
