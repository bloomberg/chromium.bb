// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_

#include "base/timer/timer.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/display_change_notifier.h"
#include "ui/gfx/screen.h"
#include "ui/views/views_export.h"

namespace gfx {
class Display;
}

typedef unsigned long XID;
typedef XID Window;
typedef struct _XDisplay Display;

namespace views {
class DesktopScreenX11Test;

// Our singleton screen implementation that talks to xrandr.
class VIEWS_EXPORT DesktopScreenX11 : public gfx::Screen,
                                      public ui::PlatformEventDispatcher {
 public:
  DesktopScreenX11();

  virtual ~DesktopScreenX11();

  // Overridden from gfx::Screen:
  virtual gfx::Point GetCursorScreenPoint() override;
  virtual gfx::NativeWindow GetWindowUnderCursor() override;
  virtual gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point)
      override;
  virtual int GetNumDisplays() const override;
  virtual std::vector<gfx::Display> GetAllDisplays() const override;
  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView window) const override;
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  virtual gfx::Display GetPrimaryDisplay() const override;
  virtual void AddObserver(gfx::DisplayObserver* observer) override;
  virtual void RemoveObserver(gfx::DisplayObserver* observer) override;

  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

 private:
  friend class DesktopScreenX11Test;

  // Constructor used in tests.
  DesktopScreenX11(const std::vector<gfx::Display>& test_displays);

  // Builds a list of displays from the current screen information offered by
  // the X server.
  std::vector<gfx::Display> BuildDisplaysFromXRandRInfo();

  // We delay updating the display so we can coalesce events.
  void ConfigureTimerFired();

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
  scoped_ptr<base::OneShotTimer<DesktopScreenX11> > configure_timer_;

  gfx::DisplayChangeNotifier change_notifier_;

  DISALLOW_COPY_AND_ASSIGN(DesktopScreenX11);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_SCREEN_X11_H_
