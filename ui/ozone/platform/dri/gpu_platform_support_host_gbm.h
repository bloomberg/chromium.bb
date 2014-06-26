// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_GPU_PLATFORM_SUPPORT_HOST_GBM_H_
#define UI_OZONE_PLATFORM_DRI_GPU_PLATFORM_SUPPORT_HOST_GBM_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/platform/dri/hardware_cursor_delegate.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {

class OZONE_EXPORT GpuPlatformSupportHostGbm : public GpuPlatformSupportHost,
                                               public HardwareCursorDelegate {
 public:
  GpuPlatformSupportHostGbm();

  // GpuPlatformSupportHost:
  virtual void OnChannelEstablished(int host_id, IPC::Sender* sender) OVERRIDE;
  virtual void OnChannelDestroyed(int host_id) OVERRIDE;

  // IPC::Listener:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // HardwareCursorDelegate:
  virtual void SetHardwareCursor(gfx::AcceleratedWidget widget,
                                 const SkBitmap& bitmap,
                                 const gfx::Point& location) OVERRIDE;
  virtual void MoveHardwareCursor(gfx::AcceleratedWidget widget,
                                  const gfx::Point& location) OVERRIDE;

 private:
  int host_id_;
  IPC::Sender* sender_;
};

}  // namespace ui

#endif  // UI_OZONE_GPU_GPU_PLATFORM_SUPPORT_HOST_GBM_H_
