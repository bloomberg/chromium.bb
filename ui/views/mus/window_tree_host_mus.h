// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_WINDOW_TREE_HOST_MUS_H_
#define UI_VIEWS_MUS_WINDOW_TREE_HOST_MUS_H_

#include "base/macros.h"
#include "components/mus/public/cpp/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"
#include "ui/gfx/geometry/rect.h"

class SkBitmap;

namespace mojo {
class Shell;
}

namespace ui {
class Compositor;
}

namespace views {

class InputMethodMUS;
class SurfaceContextFactory;

class WindowTreeHostMojo : public aura::WindowTreeHost,
                           public mus::WindowObserver {
 public:
  WindowTreeHostMojo(mojo::Shell* shell, mus::Window* window);
  ~WindowTreeHostMojo() override;

  mus::Window* mus_window() { return mus_window_; }

  ui::EventDispatchDetails SendEventToProcessor(ui::Event* event) {
    return ui::EventSource::SendEventToProcessor(event);
  }

 private:
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

  // mus::WindowObserver:
  void OnWindowBoundsChanged(mus::Window* window,
                             const mojo::Rect& old_bounds,
                             const mojo::Rect& new_bounds) override;
  void OnWindowFocusChanged(mus::Window* gained_focus,
                            mus::Window* lost_focus) override;
  void OnWindowInputEvent(mus::Window* view,
                          const mojo::EventPtr& event) override;

  mus::Window* mus_window_;

  scoped_ptr<InputMethodMUS> input_method_;

  scoped_ptr<SurfaceContextFactory> context_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMojo);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_WINDOW_TREE_HOST_MUS_H_
