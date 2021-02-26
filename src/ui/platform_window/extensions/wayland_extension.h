// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_WAYLAND_EXTENSION_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_WAYLAND_EXTENSION_H_

#include "base/component_export.h"

namespace ui {

class PlatformWindow;

class COMPONENT_EXPORT(PLATFORM_WINDOW) WaylandExtension {
 public:
  // Starts a window dragging session for the owning platform window, if
  // it is not running yet. Under Wayland, window dragging is backed by a
  // platform drag-and-drop session.
  virtual void StartWindowDraggingSessionIfNeeded() = 0;

 protected:
  virtual ~WaylandExtension();

  // Sets the pointer to the extension as a property of the PlatformWindow.
  void SetWaylandExtension(PlatformWindow* window, WaylandExtension* extension);
};

COMPONENT_EXPORT(PLATFORM_WINDOW)
WaylandExtension* GetWaylandExtension(const PlatformWindow& window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_WAYLAND_EXTENSION_H_
