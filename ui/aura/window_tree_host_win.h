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
  virtual ui::EventSource* GetEventSource() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void SetCursorNative(gfx::NativeCursor cursor) OVERRIDE;
  virtual void MoveCursorToNative(const gfx::Point& location) OVERRIDE;
  virtual void OnCursorVisibilityChangedNative(bool show) OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) OVERRIDE;

  // ui::EventSource:
  virtual ui::EventProcessor* GetEventProcessor() OVERRIDE;

 protected:
  gfx::AcceleratedWidget hwnd() const { return widget_; }

 private:
  // ui::PlatformWindowDelegate:
  virtual void OnBoundsChanged(const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnDamageRect(const gfx::Rect& damaged_region) OVERRIDE;
  virtual void DispatchEvent(ui::Event* event) OVERRIDE;
  virtual void OnCloseRequest() OVERRIDE;
  virtual void OnClosed() OVERRIDE;
  virtual void OnWindowStateChanged(ui::PlatformWindowState new_state) OVERRIDE;
  virtual void OnLostCapture() OVERRIDE;
  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) OVERRIDE;
  virtual void OnActivationChanged(bool active) OVERRIDE;

  bool has_capture_;
  gfx::Rect bounds_;
  gfx::AcceleratedWidget widget_;
  scoped_ptr<ui::PlatformWindow> window_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostWin);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_WIN_H_
