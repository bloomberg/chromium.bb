// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_GPU_PLATFORM_SUPPORT_GBM_H_
#define UI_OZONE_PLATFORM_DRI_GPU_PLATFORM_SUPPORT_GBM_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/public/gpu_platform_support.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {

class DriSurfaceFactory;

class OZONE_EXPORT GpuPlatformSupportGbm : public GpuPlatformSupport {
 public:
  GpuPlatformSupportGbm(DriSurfaceFactory* dri);

  // GpuPlatformSupport:
  virtual void OnChannelEstablished(IPC::Sender* sender) OVERRIDE;

  // IPC::Listener:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  IPC::Sender* sender_;

  void OnCursorSet(gfx::AcceleratedWidget widget,
                   const SkBitmap& bitmap,
                   const gfx::Point& location);
  void OnCursorMove(gfx::AcceleratedWidget widget, const gfx::Point& location);

  DriSurfaceFactory* dri_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_GPU_PLATFORM_SUPPORT_GBM_H_
