// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_EXTENSIONS_WAYLAND_EXTENSION_H_
#define UI_PLATFORM_WINDOW_EXTENSIONS_WAYLAND_EXTENSION_H_

#include "base/component_export.h"

namespace ui {

class PlatformWindow;

enum class WaylandWindowSnapDirection {
  kNone,
  kPrimary,
  kSecondary,
};

enum class WaylandOrientationLockType {
  kAny,
  kNatural,
  kPortrait,
  kLandscape,
  kPortraitPrimary,
  kLandscapePrimary,
  kPortraitSecondary,
  kLandscapeSecondary,
};

class COMPONENT_EXPORT(PLATFORM_WINDOW) WaylandExtension {
 public:
  // Starts a window dragging session for the owning platform window, if
  // it is not running yet. Under Wayland, window dragging is backed by a
  // platform drag-and-drop session.
  virtual void StartWindowDraggingSessionIfNeeded() = 0;

  // Signals the underneath platform that browser is entering (or exiting)
  // 'immersive fullscreen mode'.
  // Under lacros, it controls for instance interaction with the system shelf
  // widget, when browser goes in fullscreen.
  virtual void SetImmersiveFullscreenStatus(bool status) = 0;

  // Signals the underneath platform to shows a preview for the given window
  // snap direction. `allow_haptic_feedback` indicates if it should send haptic
  // feedback.
  virtual void ShowSnapPreview(WaylandWindowSnapDirection snap,
                               bool allow_haptic_feedback) = 0;

  // Requests the underneath platform to snap the window in the given direction,
  // if not WaylandWindowSnapDirection::kNone, otherwise cancels the window
  // snapping.
  virtual void CommitSnap(WaylandWindowSnapDirection snap) = 0;

  // Signals the underneath platform whether the current tab of the browser
  // window can go back. The underneath platform might react, for example,
  // by minimizing the window upon a system wide back gesture.
  virtual void SetCanGoBack(bool value) = 0;

  // Requests the underneath platform to set the window to picture-in-picture
  // (PIP).
  virtual void SetPip() = 0;

  // Whether or not the underlying platform supports native pointer locking.
  virtual bool SupportsPointerLock() = 0;
  virtual void LockPointer(bool enabled) = 0;

  // Lock and unlock the window rotation.
  virtual void Lock(WaylandOrientationLockType lock_Type) = 0;
  virtual void Unlock() = 0;

  // Retrieve current layout state.
  virtual bool GetTabletMode() = 0;

 protected:
  virtual ~WaylandExtension();

  // Sets the pointer to the extension as a property of the PlatformWindow.
  void SetWaylandExtension(PlatformWindow* window, WaylandExtension* extension);
};

COMPONENT_EXPORT(PLATFORM_WINDOW)
WaylandExtension* GetWaylandExtension(const PlatformWindow& window);

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_EXTENSIONS_WAYLAND_EXTENSION_H_
