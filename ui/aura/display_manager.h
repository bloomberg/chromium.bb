// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DISPLAY_MANAGER_H_
#define UI_AURA_DISPLAY_MANAGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "ui/aura/aura_export.h"

namespace gfx {
class Display;
class Point;
class Rect;
class Size;
}

namespace aura {
class DisplayObserver;
class RootWindow;
class Window;

// DisplayManager creates, deletes and updates Display objects when
// display configuration changes, and notifies DisplayObservers about
// the change. This is owned by Env and its lifetime is longer than
// any windows.
class AURA_EXPORT DisplayManager {
 public:
  static void set_use_fullscreen_host_window(bool use_fullscreen) {
    use_fullscreen_host_window_ = use_fullscreen;
  }
  static bool use_fullscreen_host_window() {
    return use_fullscreen_host_window_;
  }

  // Creates a display from string spec. 100+200-1440x800 creates display
  // whose size is 1440x800 at the location (100, 200) in screen's coordinates.
  // The location can be omitted and be just "1440x800", which creates
  // display at the origin of the screen. An empty string creates
  // the display with default size.
  //  The device scale factor can be specified by "*", like "1280x780*2",
  // or will use the value of |gfx::Display::GetForcedDeviceScaleFactor()| if
  // --force-device-scale-factor is specified.
  static gfx::Display CreateDisplayFromSpec(const std::string& spec);

  // A utility function to create a root window for primary display.
  static RootWindow* CreateRootWindowForPrimaryDisplay();

  DisplayManager();
  virtual ~DisplayManager();

  // Adds/removes DisplayObservers.
  void AddObserver(DisplayObserver* observer);
  void RemoveObserver(DisplayObserver* observer);

  // Called when display configuration has changed. The new display
  // configurations is passed as a vector of Display object, which
  // contains each display's new infomration.
  virtual void OnNativeDisplaysChanged(
      const std::vector<gfx::Display>& display) = 0;

  // Create a root window for given |display|.
  virtual RootWindow* CreateRootWindowForDisplay(
      const gfx::Display& display) = 0;

  // Returns the display at |index|. The display at 0 is considered "primary".
  virtual gfx::Display* GetDisplayAt(size_t index) = 0;

  virtual size_t GetNumDisplays() const = 0;

  // Returns the display object nearest given |window|.
  virtual const gfx::Display& GetDisplayNearestWindow(
      const Window* window) const = 0;

  // Returns the display object nearest given |point|.
  virtual const gfx::Display& GetDisplayNearestPoint(
      const gfx::Point& point) const = 0;

  // Returns the display that most closely intersects |match_rect|.
  virtual const gfx::Display& GetDisplayMatching(
      const gfx::Rect& match_rect) const = 0;

  // Returns the human-readable name for the display specified by |index|.
  virtual std::string GetDisplayNameAt(size_t index) = 0;

 protected:
  // Calls observers' OnDisplayBoundsChanged methods.
  void NotifyBoundsChanged(const gfx::Display& display);
  void NotifyDisplayAdded(const gfx::Display& display);
  void NotifyDisplayRemoved(const gfx::Display& display);

 private:
  // If set before the RootWindow is created, the host window will cover the
  // entire display.  Note that this can still be overridden via the
  // switches::kAuraHostWindowSize flag.
  static bool use_fullscreen_host_window_;

  ObserverList<DisplayObserver> observers_;
  DISALLOW_COPY_AND_ASSIGN(DisplayManager);
};

}  // namespace aura

#endif  // UI_AURA_DISPLAY_MANAGER_H_
