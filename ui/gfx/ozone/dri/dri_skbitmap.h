// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OZONE_DRI_DRI_SKBITMAP_H_
#define UI_GFX_OZONE_DRI_DRI_SKBITMAP_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Extend the SkBitmap interface to keep track of additional parameters used by
// the DRM stack when allocating buffers.
class GFX_EXPORT DriSkBitmap : public SkBitmap {
 public:
  DriSkBitmap(int fd);
  virtual ~DriSkBitmap();

  // Allocates the backing pixels using DRI.
  // Return true on success, false otherwise.
  virtual bool Initialize();

  uint32_t get_handle() const { return handle_; };

  uint32_t get_framebuffer() const { return framebuffer_; };

  int get_fd() const { return fd_; };

  // Return the color depth of a pixel in this buffer.
  uint8_t GetColorDepth() const;

 private:
  friend class DriAllocator;
  friend class HardwareDisplayController;

  void set_handle(uint32_t handle) { handle_ = handle; };
  void set_framebuffer(uint32_t framebuffer) { framebuffer_ = framebuffer; };

  // File descriptor used by the DRI allocator to request buffers from the DRI
  // stack.
  int fd_;

  // Buffer handle used by the DRI allocator.
  uint32_t handle_;

  // Buffer ID used by the DRI modesettings API. This is set when the buffer is
  // registered with the CRTC.
  uint32_t framebuffer_;

  DISALLOW_COPY_AND_ASSIGN(DriSkBitmap);
};

}  // namespace gfx

#endif  // UI_GFX_OZONE_DRI_DRI_SKBITMAP_H_
