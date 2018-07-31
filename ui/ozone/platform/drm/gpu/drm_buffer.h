// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_DRM_BUFFER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_DRM_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"

class SkCanvas;
struct SkImageInfo;
class SkSurface;

namespace ui {

class DrmDevice;

// Wrapper for a DRM allocated buffer. Keeps track of the native properties of
// the buffer and wraps the pixel memory into a SkSurface which can be used to
// draw into using Skia.
class DrmBuffer {
 public:
  DrmBuffer(const scoped_refptr<DrmDevice>& drm);
  ~DrmBuffer();

  // Allocates the backing pixels and wraps them in |surface_|. |info| is used
  // to describe the buffer characteristics (size, color format).
  // |should_register_framebuffer| is used to distinguish the buffers that are
  // used for modesetting.
  bool Initialize(const SkImageInfo& info, bool should_register_framebuffer);

  SkCanvas* GetCanvas() const;

  uint32_t GetHandle() const;
  gfx::Size GetSize() const;

  const scoped_refptr<DrmFramebuffer>& framebuffer() const {
    return framebuffer_;
  }

 protected:
  const scoped_refptr<DrmDevice> drm_;
  scoped_refptr<DrmFramebuffer> framebuffer_;

  // Length of a row of pixels.
  uint32_t stride_ = 0;

  // Buffer handle used by the DRM allocator.
  uint32_t handle_ = 0;

  // Base address for memory mapping.
  void* mmap_base_ = 0;

  // Size for memory mapping.
  size_t mmap_size_ = 0;

  // Wrapper around the native pixel memory.
  sk_sp<SkSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(DrmBuffer);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_DRM_BUFFER_H_
