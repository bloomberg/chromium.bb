// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_OZONE_H_
#define UI_AURA_WINDOW_TREE_HOST_OZONE_H_

#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"
#include "ui/gfx/rect.h"
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
  virtual ~WindowTreeHostOzone();

 private:
  // ui::PlatformWindowDelegate:
  virtual void OnBoundsChanged(const gfx::Rect&) override;
  virtual void OnDamageRect(const gfx::Rect& damaged_region) override;
  virtual void DispatchEvent(ui::Event* event) override;
  virtual void OnCloseRequest() override;
  virtual void OnClosed() override;
  virtual void OnWindowStateChanged(ui::PlatformWindowState new_state) override;
  virtual void OnLostCapture() override;
  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) override;
  virtual void OnActivationChanged(bool active) override;

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
  virtual void SetCursorNative(gfx::NativeCursor cursor_type) override;
  virtual void MoveCursorToNative(const gfx::Point& location) override;
  virtual void OnCursorVisibilityChangedNative(bool show) override;

  // ui::EventSource overrides.
  virtual ui::EventProcessor* GetEventProcessor() override;

  // Platform-specific part of this WindowTreeHost.
  scoped_ptr<ui::PlatformWindow> platform_window_;

  // The identifier used to create a compositing surface.
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostOzone);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_OZONE_H_
