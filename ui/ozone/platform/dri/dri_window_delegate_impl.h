// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_IMPL_H_
#define UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/dri/dri_window_delegate.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

class HardwareDisplayController;
class ScreenManager;

class DriWindowDelegateImpl : public DriWindowDelegate {
 public:
  DriWindowDelegateImpl(gfx::AcceleratedWidget widget,
                        ScreenManager* screen_manager);
  virtual ~DriWindowDelegateImpl();

  // DriWindowDelegate:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual HardwareDisplayController* GetController() OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& bounds) OVERRIDE;

 private:
  gfx::AcceleratedWidget widget_;

  ScreenManager* screen_manager_;  // Not owned.

  base::WeakPtr<HardwareDisplayController> controller_;

  DISALLOW_COPY_AND_ASSIGN(DriWindowDelegateImpl);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_IMPL_H_
