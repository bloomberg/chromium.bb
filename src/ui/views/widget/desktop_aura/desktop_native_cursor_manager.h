// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_CURSOR_MANAGER_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_CURSOR_MANAGER_H_

#include <memory>
#include <set>

#include "ui/aura/cursor/cursor_loader.h"
#include "ui/views/views_export.h"
#include "ui/wm/core/native_cursor_manager.h"

namespace aura {
class WindowTreeHost;
}

namespace wm {
class NativeCursorManagerDelegate;
}

namespace views {

// A NativeCursorManager that performs the desktop-specific setting of cursor
// state. Similar to NativeCursorManagerAsh, it also communicates these changes
// to all root windows.
class VIEWS_EXPORT DesktopNativeCursorManager : public wm::NativeCursorManager {
 public:
  DesktopNativeCursorManager();

  DesktopNativeCursorManager(const DesktopNativeCursorManager&) = delete;
  DesktopNativeCursorManager& operator=(const DesktopNativeCursorManager&) =
      delete;

  ~DesktopNativeCursorManager() override;

  // Adds |host| to the set |hosts_|.
  void AddHost(aura::WindowTreeHost* host);

  // Removes |host| from the set |hosts_|.
  void RemoveHost(aura::WindowTreeHost* host);

 private:
  // Overridden from wm::NativeCursorManager:
  void SetDisplay(const display::Display& display,
                  wm::NativeCursorManagerDelegate* delegate) override;
  void SetCursor(gfx::NativeCursor cursor,
                 wm::NativeCursorManagerDelegate* delegate) override;
  void SetVisibility(bool visible,
                     wm::NativeCursorManagerDelegate* delegate) override;
  void SetCursorSize(ui::CursorSize cursor_size,
                     wm::NativeCursorManagerDelegate* delegate) override;
  void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) override;

  // The set of hosts to notify of changes in cursor state.
  using Hosts = std::set<aura::WindowTreeHost*>;
  Hosts hosts_;

  aura::CursorLoader cursor_loader_;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_CURSOR_MANAGER_H_
