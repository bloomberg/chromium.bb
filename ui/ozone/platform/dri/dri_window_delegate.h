// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_H_
#define UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_H_

#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

class HardwareDisplayController;

// Interface for the display-server half of a DriWindow.
//
// The main implementation of this lives in the process that owns the display
// connection (usually the GPU process) and associates a platform window
// (DriWindow) with a display. A window is associated with the display whose
// bounds contains the window bounds. If there's no suitable display, the window
// is disconnected and its contents will not be visible.
//
// In software mode, this is owned directly on DriWindow because the display
// controller object is in the same process.
//
// In accelerated mode, there's a proxy implementation and calls are forwarded
// to the real object in the GPU process via IPC.
class DriWindowDelegate {
 public:
  virtual ~DriWindowDelegate() {}

  virtual void Initialize() = 0;

  virtual void Shutdown() = 0;

  // Returns the accelerated widget associated with the delegate.
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() = 0;

  // Returns the current controller the window is displaying on. Callers should
  // not cache the result as the controller may change as the window is moved.
  virtual HardwareDisplayController* GetController() = 0;

  // Called when the window is resized/moved.
  virtual void OnBoundsChanged(const gfx::Rect& bounds) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_WINDOW_DELEGATE_H_
