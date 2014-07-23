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
  virtual void OnBoundsChanged(const gfx::Rect&) OVERRIDE;
  virtual void OnDamageRect(const gfx::Rect& damaged_region) OVERRIDE;
  virtual void DispatchEvent(ui::Event* event) OVERRIDE;
  virtual void OnCloseRequest() OVERRIDE;
  virtual void OnClosed() OVERRIDE;
  virtual void OnWindowStateChanged(ui::PlatformWindowState new_state) OVERRIDE;
  virtual void OnLostCapture() OVERRIDE;
  virtual void OnAcceleratedWidgetAvailable(
      gfx::AcceleratedWidget widget) OVERRIDE;

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
  virtual void PostNativeEvent(const base::NativeEvent& event) OVERRIDE;
  virtual void SetCursorNative(gfx::NativeCursor cursor_type) OVERRIDE;
  virtual void MoveCursorToNative(const gfx::Point& location) OVERRIDE;
  virtual void OnCursorVisibilityChangedNative(bool show) OVERRIDE;

  // ui::EventSource overrides.
  virtual ui::EventProcessor* GetEventProcessor() OVERRIDE;

  // Platform-specific part of this WindowTreeHost.
  scoped_ptr<ui::PlatformWindow> platform_window_;

  // The identifier used to create a compositing surface.
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostOzone);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_OZONE_H_
