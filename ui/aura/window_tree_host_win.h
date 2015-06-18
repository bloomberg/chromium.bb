// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_WIN_H_
#define UI_AURA_WINDOW_TREE_HOST_WIN_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_tree_host.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace aura {

class AURA_EXPORT WindowTreeHostWin
    : public WindowTreeHost,
      public NON_EXPORTED_BASE(ui::PlatformWindowDelegate) {
 public:
  explicit WindowTreeHostWin(const gfx::Rect& bounds);
  ~WindowTreeHostWin() override;

  // WindowTreeHost:
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBounds() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Point GetLocationOnNativeScreen() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void SetCursorNative(gfx::NativeCursor cursor) override;
  void MoveCursorToNative(const gfx::Point& location) override;
  void OnCursorVisibilityChangedNative(bool show) override;

 protected:
  gfx::AcceleratedWidget hwnd() const { return widget_; }

 private:
  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override;
  void OnDamageRect(const gfx::Rect& damaged_region) override;
  void DispatchEvent(ui::Event* event) override;
  void OnCloseRequest() override;
  void OnClosed() override;
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override;
  void OnLostCapture() override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget,
                                    float device_pixel_ratio) override;
  void OnActivationChanged(bool active) override;

  bool has_capture_;
  gfx::Rect bounds_;
  gfx::AcceleratedWidget widget_;
  scoped_ptr<ui::PlatformWindow> window_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostWin);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_WIN_H_
