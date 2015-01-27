// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_OZONE_H_
#define UI_AURA_WINDOW_TREE_HOST_OZONE_H_

#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {
class PlatformWindow;
}

namespace aura {

class AURA_EXPORT WindowTreeHostOzone : public WindowTreeHost,
                                        public ui::EventSource,
                                        public ui::PlatformWindowDelegate {
 public:
  explicit WindowTreeHostOzone(const gfx::Rect& bounds);
  ~WindowTreeHostOzone() override;

 protected:
  // WindowTreeHost:
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void Show() override;
  void Hide() override;
  gfx::Rect GetBounds() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Point GetLocationOnNativeScreen() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void SetCursorNative(gfx::NativeCursor cursor_type) override;
  void MoveCursorToNative(const gfx::Point& location) override;
  void OnCursorVisibilityChangedNative(bool show) override;

  ui::PlatformWindow* platform_window() { return platform_window_.get(); }

 private:
  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect&) override;
  void OnDamageRect(const gfx::Rect& damaged_region) override;
  void DispatchEvent(ui::Event* event) override;
  void OnCloseRequest() override;
  void OnClosed() override;
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override;
  void OnLostCapture() override;
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override;
  void OnActivationChanged(bool active) override;

  // ui::EventSource overrides.
  ui::EventProcessor* GetEventProcessor() override;

  // Platform-specific part of this WindowTreeHost.
  scoped_ptr<ui::PlatformWindow> platform_window_;

  // The identifier used to create a compositing surface.
  gfx::AcceleratedWidget widget_;

  // Current Aura cursor.
  gfx::NativeCursor current_cursor_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostOzone);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_OZONE_H_
