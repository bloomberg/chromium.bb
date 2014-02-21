// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_CURSOR_MANAGER_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_CURSOR_MANAGER_H_

#include <set>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/corewm/native_cursor_manager.h"
#include "ui/views/views_export.h"

namespace aura {
class WindowEventDispatcher;
}

namespace ui {
class CursorLoader;
}

namespace views {
class DesktopCursorLoaderUpdater;

namespace corewm {
class NativeCursorManagerDelegate;
}

// A NativeCursorManager that performs the desktop-specific setting of cursor
// state. Similar to AshNativeCursorManager, it also communicates these changes
// to all root windows.
class VIEWS_EXPORT DesktopNativeCursorManager
    : public views::corewm::NativeCursorManager {
 public:
  DesktopNativeCursorManager(
      scoped_ptr<DesktopCursorLoaderUpdater> cursor_loader_updater);
  virtual ~DesktopNativeCursorManager();

  // Builds a cursor and sets the internal platform representation.
  gfx::NativeCursor GetInitializedCursor(int type);

  // Adds |root_window| to the set |root_windows_|.
  void AddRootWindow(aura::WindowEventDispatcher* dispatcher);

  // Removes |root_window| from the set |root_windows_|.
  void RemoveRootWindow(aura::WindowEventDispatcher* dispatcher);

 private:
  // Overridden from views::corewm::NativeCursorManager:
  virtual void SetDisplay(
      const gfx::Display& display,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE;
  virtual void SetCursor(
      gfx::NativeCursor cursor,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE;
  virtual void SetVisibility(
      bool visible,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE;
  virtual void SetCursorSet(
      ui::CursorSetType cursor_set,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE;
  virtual void SetScale(
      float scale,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE;
  virtual void SetMouseEventsEnabled(
      bool enabled,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE;

  // The set of dispatchers to notify of changes in cursor state.
  typedef std::set<aura::WindowEventDispatcher*> Dispatchers;
  Dispatchers dispatchers_;

  scoped_ptr<DesktopCursorLoaderUpdater> cursor_loader_updater_;
  scoped_ptr<ui::CursorLoader> cursor_loader_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNativeCursorManager);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_CURSOR_MANAGER_H_

