// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_GBM_BUFFER_H_
#define UI_OZONE_PLATFORM_DRI_GBM_BUFFER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/dri/scanout_surface.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_factory_ozone.h"

struct gbm_bo;
struct gbm_device;

namespace ui {

class DriWrapper;

class GbmBuffer : public ScanoutSurface {
 public:
  GbmBuffer(gbm_device* device, DriWrapper* dri, const gfx::Size& size);
  virtual ~GbmBuffer();

  bool InitializeBuffer(SurfaceFactoryOzone::BufferFormat format, bool scanout);

  // ScanoutSurface:
  virtual bool Initialize() OVERRIDE;
  virtual uint32_t GetFramebufferId() const OVERRIDE;
  virtual uint32_t GetHandle() const OVERRIDE;
  virtual gfx::Size Size() const OVERRIDE;
  virtual void PreSwapBuffers() OVERRIDE;
  virtual void SwapBuffers() OVERRIDE;

  gbm_bo* bo() { return bo_; }

 private:
  gbm_device* gbm_device_;
  gbm_bo* bo_;

  uint32_t handle_;
  uint32_t framebuffer_;

  DriWrapper* dri_;

  gfx::Size size_;
};

class GbmPixmap : public NativePixmap {
 public:
  GbmPixmap(gbm_device* device, DriWrapper* dri, const gfx::Size& size);
  virtual ~GbmPixmap();

  // NativePixmap:
  virtual void* GetEGLClientBuffer() OVERRIDE;
  virtual int GetDmaBufFd() OVERRIDE;

  GbmBuffer* buffer() { return &buffer_; }

 private:
  GbmBuffer buffer_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_GBM_BUFFER_H_
