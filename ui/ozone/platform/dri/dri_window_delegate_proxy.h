// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_PROXY_H_
#define UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_PROXY_H_

#include "ui/ozone/platform/dri/dri_window_delegate.h"

namespace ui {

class GpuPlatformSupportHostGbm;

// This is used when running with a GPU process (or with the in-process GPU) to
// IPC the native window configuration from the browser to the GPU.
class DriWindowDelegateProxy : public DriWindowDelegate {
 public:
  DriWindowDelegateProxy(gfx::AcceleratedWidget widget,
                         GpuPlatformSupportHostGbm* sender);
  virtual ~DriWindowDelegateProxy();

  // DriWindowDelegate:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual HardwareDisplayController* GetController() OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& bounds) OVERRIDE;

 private:
  gfx::AcceleratedWidget widget_;
  GpuPlatformSupportHostGbm* sender_;

  DISALLOW_COPY_AND_ASSIGN(DriWindowDelegateProxy);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_PROXY_H_
