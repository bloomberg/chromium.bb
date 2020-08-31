// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_x11.h"

#include <vector>

#include "base/command_line.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/layout.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/switches.h"
#include "ui/platform_window/x11/x11_topmost_window_finder.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

namespace views {

DesktopScreenX11::DesktopScreenX11() {
  if (LinuxUI::instance())
    display_scale_factor_observer_.Add(LinuxUI::instance());
}

DesktopScreenX11::~DesktopScreenX11() = default;

void DesktopScreenX11::Init() {
  if (x11_display_manager_->IsXrandrAvailable() &&
      ui::X11EventSource::HasInstance())
    event_source_observer_.Add(ui::X11EventSource::GetInstance());
  x11_display_manager_->Init();
}

gfx::Point DesktopScreenX11::GetCursorScreenPoint() {
  TRACE_EVENT0("views", "DesktopScreenX11::GetCursorScreenPoint()");

  base::Optional<gfx::Point> point;
  if (const auto* const event_source = ui::X11EventSource::GetInstance())
    point = event_source->GetRootCursorLocationFromCurrentEvent();
  return gfx::ConvertPointToDIP(
      GetXDisplayScaleFactor(),
      // NB: Do NOT call value_or() here, since that would defeat the purpose of
      // caching |point|.
      point ? point.value() : x11_display_manager_->GetCursorLocation());
}

bool DesktopScreenX11::IsWindowUnderCursor(gfx::NativeWindow window) {
  return GetWindowAtScreenPoint(GetCursorScreenPoint()) == window;
}

gfx::NativeWindow DesktopScreenX11::GetWindowAtScreenPoint(
    const gfx::Point& point) {
  auto accelerated_widget =
      ui::X11TopmostWindowFinder().FindLocalProcessWindowAt(
          gfx::ConvertPointToPixel(GetXDisplayScaleFactor(), point), {});
  return accelerated_widget
             ? views::DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
                   static_cast<gfx::AcceleratedWidget>(accelerated_widget))
             : nullptr;
}

gfx::NativeWindow DesktopScreenX11::GetLocalProcessWindowAtPoint(
    const gfx::Point& point,
    const std::set<gfx::NativeWindow>& ignore) {
  std::set<gfx::AcceleratedWidget> ignore_widgets;
  for (auto* const window : ignore)
    ignore_widgets.emplace(window->GetHost()->GetAcceleratedWidget());
  auto accelerated_widget =
      ui::X11TopmostWindowFinder().FindLocalProcessWindowAt(
          gfx::ConvertPointToPixel(GetXDisplayScaleFactor(), point),
          ignore_widgets);
  return accelerated_widget
             ? views::DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
                   static_cast<gfx::AcceleratedWidget>(accelerated_widget))
             : nullptr;
}

int DesktopScreenX11::GetNumDisplays() const {
  return int{x11_display_manager_->displays().size()};
}

const std::vector<display::Display>& DesktopScreenX11::GetAllDisplays() const {
  return x11_display_manager_->displays();
}

display::Display DesktopScreenX11::GetDisplayNearestWindow(
    gfx::NativeView window) const {
  // Getting screen bounds here safely is hard.
  //
  // You'd think we'd be able to just call window->GetBoundsInScreen(), but we
  // can't because |window| (and the associated WindowEventDispatcher*) can be
  // partially initialized at this point; WindowEventDispatcher initializations
  // call through into GetDisplayNearestWindow(). But the X11 resources are
  // created before we create the aura::WindowEventDispatcher. So we ask what
  // the DRWHX11 believes the window bounds are instead of going through the
  // aura::Window's screen bounds.
  if (aura::WindowTreeHost* host = window ? window->GetHost() : nullptr) {
    const auto* const desktop_host =
        DesktopWindowTreeHostLinux::GetHostForWidget(
            host->GetAcceleratedWidget());
    if (desktop_host) {
      const gfx::Rect pixel_rect = desktop_host->GetBoundsInPixels();
      return GetDisplayMatching(
          gfx::ConvertRectToDIP(GetXDisplayScaleFactor(), pixel_rect));
    }
  }

  return GetPrimaryDisplay();
}

display::Display DesktopScreenX11::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return (GetNumDisplays() <= 1)
             ? GetPrimaryDisplay()
             : *FindDisplayNearestPoint(GetAllDisplays(), point);
}

display::Display DesktopScreenX11::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  const display::Display* const matching =
      display::FindDisplayWithBiggestIntersection(GetAllDisplays(), match_rect);
  return matching ? *matching : GetPrimaryDisplay();
}

display::Display DesktopScreenX11::GetPrimaryDisplay() const {
  return x11_display_manager_->GetPrimaryDisplay();
}

void DesktopScreenX11::AddObserver(display::DisplayObserver* observer) {
  x11_display_manager_->AddObserver(observer);
}

void DesktopScreenX11::RemoveObserver(display::DisplayObserver* observer) {
  x11_display_manager_->RemoveObserver(observer);
}

std::string DesktopScreenX11::GetCurrentWorkspace() {
  return x11_display_manager_->GetCurrentWorkspace();
}

bool DesktopScreenX11::DispatchXEvent(XEvent* event) {
  return x11_display_manager_->CanProcessEvent(*event) &&
         x11_display_manager_->ProcessEvent(event);
}

void DesktopScreenX11::OnDeviceScaleFactorChanged() {
  x11_display_manager_->DispatchDelayedDisplayListUpdate();
}

// static
void DesktopScreenX11::UpdateDeviceScaleFactorForTest() {
  auto* screen = static_cast<DesktopScreenX11*>(display::Screen::GetScreen());
  screen->x11_display_manager_->UpdateDisplayList();
}

void DesktopScreenX11::OnXDisplayListUpdated() {
  gfx::SetFontRenderParamsDeviceScaleFactor(
      GetPrimaryDisplay().device_scale_factor());
}

float DesktopScreenX11::GetXDisplayScaleFactor() const {
  if (LinuxUI::instance())
    return LinuxUI::instance()->GetDeviceScaleFactor();
  return display::Display::HasForceDeviceScaleFactor()
             ? display::Display::GetForcedDeviceScaleFactor()
             : 1.0f;
}

////////////////////////////////////////////////////////////////////////////////

display::Screen* CreateDesktopScreen() {
  auto* screen = new DesktopScreenX11;
  screen->Init();
  return screen;
}

}  // namespace views
