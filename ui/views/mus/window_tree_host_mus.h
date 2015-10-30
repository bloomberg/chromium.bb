// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_WINDOW_TREE_HOST_MUS_H_
#define UI_VIEWS_MUS_WINDOW_TREE_HOST_MUS_H_

#include "base/macros.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
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

class WindowTreeHostMus : public aura::WindowTreeHost,
                          public mus::WindowObserver {
 public:
  WindowTreeHostMus(mojo::Shell* shell, mus::Window* window);
  ~WindowTreeHostMus() override;

  mus::Window* mus_window() { return mus_window_; }

  mus::mojom::ShowState show_state() const { return show_state_; }
  void SetShowState(mus::mojom::ShowState show_state);

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
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowFocusChanged(mus::Window* gained_focus,
                            mus::Window* lost_focus) override;
  void OnWindowInputEvent(mus::Window* view,
                          const mus::mojom::EventPtr& event) override;
  void OnWindowSharedPropertyChanged(
      mus::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override;

  mus::Window* mus_window_;

  mus::mojom::ShowState show_state_;

  scoped_ptr<InputMethodMUS> input_method_;

  scoped_ptr<SurfaceContextFactory> context_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMus);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_WINDOW_TREE_HOST_MUS_H_
