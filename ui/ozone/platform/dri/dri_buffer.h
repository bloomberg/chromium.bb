// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_BUFFER_H_
#define UI_OZONE_PLATFORM_DRI_DRI_BUFFER_H_

#include "base/macros.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/platform/dri/scanout_buffer.h"

namespace ui {

class DriWrapper;

// Wrapper for a DRM allocated buffer. Keeps track of the native properties of
// the buffer and wraps the pixel memory into a SkSurface which can be used to
// draw into using Skia.
class DriBuffer : public ScanoutBuffer {
 public:
  DriBuffer(DriWrapper* dri);

  // Allocates the backing pixels and wraps them in |surface_|. |info| is used
  // to describe the buffer characteristics (size, color format).
  bool Initialize(const SkImageInfo& info);

  SkCanvas* GetCanvas() const;

  // ScanoutBuffer:
  virtual uint32_t GetFramebufferId() const OVERRIDE;
  virtual uint32_t GetHandle() const OVERRIDE;
  virtual gfx::Size GetSize() const OVERRIDE;

 protected:
  virtual ~DriBuffer();

  DriWrapper* dri_;  // Not owned.

  // Wrapper around the native pixel memory.
  skia::RefPtr<SkSurface> surface_;

  // Length of a row of pixels.
  uint32_t stride_;

  // Buffer handle used by the DRM allocator.
  uint32_t handle_;

  // Buffer ID used by the DRM modesettings API. This is set when the buffer is
  // registered with the CRTC.
  uint32_t framebuffer_;

  DISALLOW_COPY_AND_ASSIGN(DriBuffer);
};

class DriBufferGenerator : public ScanoutBufferGenerator {
 public:
  DriBufferGenerator(DriWrapper* dri);
  virtual ~DriBufferGenerator();

  // ScanoutBufferGenerator:
  virtual scoped_refptr<ScanoutBuffer> Create(const gfx::Size& size) OVERRIDE;

 private:
  DriWrapper* dri_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(DriBufferGenerator);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_BUFFER_H_
