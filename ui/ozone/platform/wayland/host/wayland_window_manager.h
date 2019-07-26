// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_MANAGER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;

// Stores and returns WaylandWindows. Clients that are interested in knowing
// when a new window is added or removed, but set self as an observer.
class WaylandWindowManager {
 public:
  // TODO(msisov): Do not pass WaylandConnection here. Instead, add support for
  // observers.
  explicit WaylandWindowManager(WaylandConnection* connection);
  ~WaylandWindowManager();

  // Returns a window found by |widget|.
  WaylandWindow* GetWindow(gfx::AcceleratedWidget widget) const;

  // Returns a window with largests bounds.
  WaylandWindow* GetWindowWithLargestBounds() const;

  // Returns a current focused window by pointer.
  WaylandWindow* GetCurrentFocusedWindow() const;

  // Returns a current focused window by keyboard.
  WaylandWindow* GetCurrentKeyboardFocusedWindow() const;

  // TODO(crbug.com/971525): remove this in favor of targeted subscription of
  // windows to their outputs.
  std::vector<WaylandWindow*> GetWindowsOnOutput(uint32_t output_id);

  // Returns all stored windows.
  std::vector<WaylandWindow*> GetAllWindows() const;

  void AddWindow(gfx::AcceleratedWidget widget, WaylandWindow* window);
  void RemoveWindow(gfx::AcceleratedWidget widget);

 private:
  WaylandConnection* const connection_;

  base::flat_map<gfx::AcceleratedWidget, WaylandWindow*> window_map_;

  DISALLOW_COPY_AND_ASSIGN(WaylandWindowManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_WINDOW_MANAGER_H_
