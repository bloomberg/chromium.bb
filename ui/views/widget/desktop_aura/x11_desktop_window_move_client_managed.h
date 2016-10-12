// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_X11_DESKTOP_WINDOW_MOVE_CLIENT_MANAGED_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_X11_DESKTOP_WINDOW_MOVE_CLIENT_MANAGED_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/views/views_export.h"
#include "ui/wm/public/window_move_client.h"

typedef unsigned long XID;

namespace ui {
class ScopedEventDispatcher;
}

namespace views {

class VIEWS_EXPORT X11DesktopWindowMoveClientManaged
    : public aura::client::WindowMoveClient,
      public ui::PlatformEventDispatcher {
 public:
  X11DesktopWindowMoveClientManaged();
  ~X11DesktopWindowMoveClientManaged() override;

  // Overridden from ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  // Overridden from aura::client::WindowMoveClient:
  aura::client::WindowMoveResult RunMoveLoop(
      aura::Window* window,
      const gfx::Vector2d& drag_offset,
      aura::client::WindowMoveSource move_source) override;
  void EndMoveLoop() override;

 private:
  // Are we running a nested message loop from RunMoveLoop()?
  bool in_move_loop_;
  std::unique_ptr<ui::ScopedEventDispatcher> nested_dispatcher_;

  XID window_;

  base::Closure quit_closure_;

  base::WeakPtrFactory<X11DesktopWindowMoveClientManaged> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(X11DesktopWindowMoveClientManaged);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_X11_DESKTOP_WINDOW_MOVE_CLIENT_MANAGED_H_
