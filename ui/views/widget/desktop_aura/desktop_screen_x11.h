// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/display_change_notifier.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/views/views_export.h"

namespace gfx {
class Display;
}

typedef unsigned long XID;
typedef XID Window;
typedef struct _XDisplay Display;

namespace views {
class DesktopScreenX11Test;

namespace test {
class DesktopScreenX11TestApi;
}

// Our singleton screen implementation that talks to xrandr.
class VIEWS_EXPORT DesktopScreenX11 : public gfx::Screen,
                                      public ui::PlatformEventDispatcher {
 public:
  DesktopScreenX11();

  ~DesktopScreenX11() override;

  // Overridden from gfx::Screen:
  gfx::Point GetCursorScreenPoint() override;
  gfx::NativeWindow GetWindowUnderCursor() override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  std::vector<gfx::Display> GetAllDisplays() const override;
  gfx::Display GetDisplayNearestWindow(gfx::NativeView window) const override;
  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const override;
  gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  gfx::Display GetPrimaryDisplay() const override;
  void AddObserver(gfx::DisplayObserver* observer) override;
  void RemoveObserver(gfx::DisplayObserver* observer) override;

  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  static void UpdateDeviceScaleFactorForTest();

 private:
  friend class DesktopScreenX11Test;
  friend class test::DesktopScreenX11TestApi;

  // Constructor used in tests.
  DesktopScreenX11(const std::vector<gfx::Display>& test_displays);

  // Builds a list of displays from the current screen information offered by
  // the X server.
  std::vector<gfx::Display> BuildDisplaysFromXRandRInfo();

  // We delay updating the display so we can coalesce events.
  void ConfigureTimerFired();

  // Updates |displays_| and sets FontRenderParams's scale factor.
  void SetDisplaysInternal(const std::vector<gfx::Display>& displays);

  Display* xdisplay_;
  ::Window x_root_window_;

  // Whether the x server supports the XRandR extension.
  bool has_xrandr_;

  // The base of the event numbers used to represent XRandr events used in
  // decoding events regarding output add/remove.
  int xrandr_event_base_;

  // The display objects we present to chrome.
  std::vector<gfx::Display> displays_;

  // The timer to delay configuring outputs. See also the comments in
  // Dispatch().
  std::unique_ptr<base::OneShotTimer> configure_timer_;

  gfx::DisplayChangeNotifier change_notifier_;

  ui::X11AtomCache atom_cache_;

  DISALLOW_COPY_AND_ASSIGN(DesktopScreenX11);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_
