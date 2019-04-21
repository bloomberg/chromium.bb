// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/display/display_list.h"
#include "ui/ozone/platform/x11/x11_display_fetcher_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_screen.h"

namespace ui {

// A PlatformScreen implementation for X11.
class X11ScreenOzone : public PlatformScreen,
                       public X11DisplayFetcherOzone::Delegate {
 public:
  X11ScreenOzone();
  ~X11ScreenOzone() override;

  // PlatformScreen implementation.
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetPrimaryDisplay() const override;
  display::Display GetDisplayForAcceleratedWidget(
      gfx::AcceleratedWidget widget) const override;
  gfx::Point GetCursorScreenPoint() const override;
  gfx::AcceleratedWidget GetAcceleratedWidgetAtScreenPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

  // X11DisplayFetcherOzone::Delegate overrides:
  void AddDisplay(const display::Display& display, bool is_primary) override;
  void RemoveDisplay(const display::Display& display) override;

 private:
  display::DisplayList display_list_;

  base::ObserverList<display::DisplayObserver> observers_;

  std::unique_ptr<X11DisplayFetcherOzone> display_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(X11ScreenOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_SCREEN_OZONE_H_
