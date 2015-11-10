// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_PLATFORM_WINDOW_MUS_H_
#define UI_VIEWS_MUS_PLATFORM_WINDOW_MUS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/mus/public/cpp/window_observer.h"
#include "ui/platform_window/platform_window.h"

namespace views {

class PlatformWindowMus : public ui::PlatformWindow,
                          public mus::WindowObserver {
 public:
  PlatformWindowMus(ui::PlatformWindowDelegate* delegate,
                    mus::Window* mus_window);
  ~PlatformWindowMus() override;

  void Activate();

  // ui::PlatformWindow:
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetTitle(const base::string16& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetCursor(ui::PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  ui::PlatformImeController* GetPlatformImeController() override;

 private:
  void SetShowState(mus::mojom::ShowState show_state);

  // mus::WindowObserver:
  void OnWindowDestroyed(mus::Window* window) override;
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

  ui::PlatformWindowDelegate* delegate_;
  mus::Window* mus_window_;
  mus::mojom::ShowState show_state_;

  DISALLOW_COPY_AND_ASSIGN(PlatformWindowMus);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_PLATFORM_WINDOW_MUS_H_
