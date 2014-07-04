// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_GBM_BUFFER_H_
#define UI_OZONE_PLATFORM_DRI_GBM_BUFFER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_factory_ozone.h"

struct gbm_bo;
struct gbm_device;

namespace ui {

class DriWrapper;

class GbmBuffer : public NativePixmap {
 public:
  GbmBuffer(gbm_device* device, DriWrapper* dri, const gfx::Size& size);

  bool InitializeBuffer(SurfaceFactoryOzone::BufferFormat format, bool scanout);

  // NativePixmap:
  virtual void* GetEGLClientBuffer() OVERRIDE;
  virtual int GetDmaBufFd() OVERRIDE;

 protected:
  virtual ~GbmBuffer();

 private:
  gbm_device* gbm_device_;
  gbm_bo* bo_;

  uint32_t handle_;
  uint32_t framebuffer_;

  DriWrapper* dri_;

  gfx::Size size_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_GBM_BUFFER_H_
