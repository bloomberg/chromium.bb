// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_PLATFORM_WINDOW_MUS_H_
#define UI_VIEWS_MUS_PLATFORM_WINDOW_MUS_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/mus/public/cpp/input_event_handler.h"
#include "components/mus/public/cpp/window_observer.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/mus/mus_export.h"

namespace bitmap_uploader {
class BitmapUploader;
}

namespace shell {
class Connector;
}

namespace ui {
class Event;
class ViewProp;
}

namespace views {

class VIEWS_MUS_EXPORT PlatformWindowMus
    : public NON_EXPORTED_BASE(ui::PlatformWindow),
      public mus::WindowObserver,
      public NON_EXPORTED_BASE(mus::InputEventHandler) {
 public:
  PlatformWindowMus(ui::PlatformWindowDelegate* delegate,
                    shell::Connector* connector,
                    mus::Window* mus_window);
  ~PlatformWindowMus() override;

  void SetCursorById(mus::mojom::Cursor cursor);

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
  friend class PlatformWindowMusTest;

  void SetShowState(mus::mojom::ShowState show_state);

  // mus::WindowObserver:
  void OnWindowDestroyed(mus::Window* window) override;
  void OnWindowBoundsChanged(mus::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowFocusChanged(mus::Window* gained_focus,
                            mus::Window* lost_focus) override;
  void OnWindowPredefinedCursorChanged(mus::Window* window,
                                       mus::mojom::Cursor cursor) override;
  void OnWindowSharedPropertyChanged(
      mus::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override;
  void OnRequestClose(mus::Window* window) override;

  // mus::InputEventHandler:
  void OnWindowInputEvent(
      mus::Window* view,
      const ui::Event& event,
      std::unique_ptr<base::Callback<void(mus::mojom::EventResult)>>*
          ack_callback) override;

  ui::PlatformWindowDelegate* delegate_;
  mus::Window* mus_window_;
  mus::mojom::ShowState show_state_;
  mus::mojom::Cursor last_cursor_;

  // True if OnWindowDestroyed() has been received.
  bool mus_window_destroyed_;

  std::unique_ptr<bitmap_uploader::BitmapUploader> bitmap_uploader_;
  std::unique_ptr<ui::ViewProp> prop_;
#ifndef NDEBUG
  std::unique_ptr<base::WeakPtrFactory<PlatformWindowMus>> weak_factory_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PlatformWindowMus);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_PLATFORM_WINDOW_MUS_H_
