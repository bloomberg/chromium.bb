// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_screen_ozone.h"

#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

X11ScreenOzone::X11ScreenOzone() {
  // Creates |display_fetcher_| instead of adding it to a member initializer
  // list as it requires |this|.
  display_fetcher_ = std::make_unique<X11DisplayFetcherOzone>(this);
}

X11ScreenOzone::~X11ScreenOzone() = default;

const std::vector<display::Display>& X11ScreenOzone::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display X11ScreenOzone::GetPrimaryDisplay() const {
  auto iter = display_list_.GetPrimaryDisplayIterator();
  if (iter == display_list_.displays().end())
    return display::Display::GetDefaultDisplay();
  return *iter;
}

display::Display X11ScreenOzone::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  // TODO(jkim): https://crbug.com/891175
  NOTIMPLEMENTED_LOG_ONCE();
  return GetPrimaryDisplay();
}

gfx::Point X11ScreenOzone::GetCursorScreenPoint() const {
  // TODO(jkim): https://crbug.com/891175
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Point();
}

gfx::AcceleratedWidget X11ScreenOzone::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  // TODO(jkim): https://crbug.com/891175
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::kNullAcceleratedWidget;
}

display::Display X11ScreenOzone::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  // TODO(jkim): https://crbug.com/891175
  NOTIMPLEMENTED_LOG_ONCE();
  return GetPrimaryDisplay();
}

display::Display X11ScreenOzone::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  // TODO(jkim): https://crbug.com/891175
  NOTIMPLEMENTED_LOG_ONCE();
  return GetPrimaryDisplay();
}

void X11ScreenOzone::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void X11ScreenOzone::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

void X11ScreenOzone::AddDisplay(const display::Display& display,
                                bool is_primary) {
  display_list_.AddDisplay(
      display, is_primary ? display::DisplayList::Type::PRIMARY
                          : display::DisplayList::Type::NOT_PRIMARY);

  if (is_primary) {
    gfx::SetFontRenderParamsDeviceScaleFactor(
        GetPrimaryDisplay().device_scale_factor());
  }
}

void X11ScreenOzone::RemoveDisplay(const display::Display& display) {
  display_list_.RemoveDisplay(display.id());
}

}  // namespace ui
