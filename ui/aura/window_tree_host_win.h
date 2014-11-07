// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_WIN_H_
#define UI_AURA_WINDOW_TREE_HOST_WIN_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace aura {

class AURA_EXPORT WindowTreeHostWin
    : public WindowTreeHost,
      public ui::EventSource,
      public NON_EXPORTED_BASE(ui::PlatformWindowDelegate) {
 public:
  explicit WindowTreeHostWin(const gfx::Rect& bounds);
  virtual ~WindowTreeHostWin();

  // WindowTreeHost:
  virtual ui::EventSource* GetEventSource() override;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual gfx::Rect GetBounds() const override;
  virtual void SetBounds(const gfx::Rect& bounds) override;
  virtual gfx::Point GetLocationOnNativeScreen() const override;
  virtual void SetCapture() override;
  virtual void ReleaseCapture() override;
  virtual void SetCursorNative(gfx::NativeCursor cursor) override;
  virtual void MoveCursorToNative(const gfx::Point& location) override;
  virtual void OnCursorVisibilityChangedNative(bool show) override;

  // ui::EventSource:
  virtual ui::EventProcessor* GetEventProcessor() override;

 protected:
  gfx::AcceleratedWidget hwnd() const { return widget_; }

 private:
  // ui::PlatformWindowDelegate:
  virtual void OnBoundsChanged(const gfx::Rect& new_bounds) override;
  virtual void OnDamageRect(const gfx::Rect& damaged_region) override;
  virtual void DispatchEvent(ui::Event* event) override;
  virtual void OnCloseRequest() override;
  virtual void OnClosed() override;
  virtual void OnWindowStateChanged(ui::PlatformWindowState new_state) override;
  virtual void OnLostCapture() override;
  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) override;
  virtual void OnActivationChanged(bool active) override;

  bool has_capture_;
  gfx::Rect bounds_;
  gfx::AcceleratedWidget widget_;
  scoped_ptr<ui::PlatformWindow> window_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostWin);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_WIN_H_
