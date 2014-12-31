// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_MAC_H_
#define UI_AURA_WINDOW_TREE_HOST_MAC_H_

#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class MouseEvent;
}

namespace aura {

namespace internal {
class TouchEventCalibrate;
}

class AURA_EXPORT WindowTreeHostMac : public WindowTreeHost {
 public:
  explicit WindowTreeHostMac(const gfx::Rect& bounds);
  virtual ~WindowTreeHostMac();

 private:
  // WindowTreeHost Overrides.
  virtual ui::EventSource* GetEventSource() override;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual void ToggleFullScreen() override;
  virtual gfx::Rect GetBounds() const override;
  virtual void SetBounds(const gfx::Rect& bounds) override;
  virtual gfx::Insets GetInsets() const override;
  virtual void SetInsets(const gfx::Insets& insets) override;
  virtual gfx::Point GetLocationOnNativeScreen() const override;
  virtual void SetCapture() override;
  virtual void ReleaseCapture() override;
  virtual bool ConfineCursorToRootWindow() override;
  virtual void UnConfineCursor() override;
  virtual void SetCursorNative(gfx::NativeCursor cursor_type) override;
  virtual void MoveCursorToNative(const gfx::Point& location) override;
  virtual void OnCursorVisibilityChangedNative(bool show) override;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) override;

 private:
  base::scoped_nsobject<NSWindow> window_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMac);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_MAC_H_
