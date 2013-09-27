// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_x11.h"

#include <X11/extensions/Xrandr.h>
#include <X11/Xlib.h>

// It clashes with out RootWindow.
#undef RootWindow

#include "base/logging.h"
#include "base/x11/edid_parser_x11.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/widget/desktop_aura/desktop_root_window_host_x11.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"

namespace {

// The delay to perform configuration after RRNotify.  See the comment
// in |Dispatch()|.
const int64 kConfigureDelayMs = 500;

std::vector<gfx::Display> GetFallbackDisplayList() {
  ::XDisplay* display = gfx::GetXDisplay();
  ::Screen* screen = DefaultScreenOfDisplay(display);
  int width = WidthOfScreen(screen);
  int height = HeightOfScreen(screen);

  return std::vector<gfx::Display>(
      1, gfx::Display(0, gfx::Rect(0, 0, width, height)));
}

}  // namespace

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenX11, public:

DesktopScreenX11::DesktopScreenX11()
    : xdisplay_(base::MessagePumpX11::GetDefaultXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      has_xrandr_(false),
      xrandr_event_base_(0) {
  // We only support 1.3+. There were library changes before this and we should
  // use the new interface instead of the 1.2 one.
  int randr_version_major = 0;
  int randr_version_minor = 0;
  has_xrandr_ = XRRQueryVersion(
        xdisplay_, &randr_version_major, &randr_version_minor) &&
      randr_version_major == 1 &&
      randr_version_minor >= 3;

  if (has_xrandr_) {
    int error_base_ignored = 0;
    XRRQueryExtension(xdisplay_, &xrandr_event_base_, &error_base_ignored);

    base::MessagePumpX11::Current()->AddDispatcherForRootWindow(this);
    XRRSelectInput(xdisplay_,
                   x_root_window_,
                   RRScreenChangeNotifyMask | RROutputChangeNotifyMask);

    displays_ = BuildDisplaysFromXRandRInfo();
  } else {
    displays_ = GetFallbackDisplayList();
  }
}

DesktopScreenX11::~DesktopScreenX11() {
  if (has_xrandr_)
    base::MessagePumpX11::Current()->RemoveDispatcherForRootWindow(this);
}

void DesktopScreenX11::ProcessDisplayChange(
    const std::vector<gfx::Display>& incoming) {
  std::vector<gfx::Display>::const_iterator cur_it = displays_.begin();
  for (; cur_it != displays_.end(); ++cur_it) {
    bool found = false;
    for (std::vector<gfx::Display>::const_iterator incoming_it =
             incoming.begin(); incoming_it != incoming.end(); ++incoming_it) {
      if (cur_it->id() == incoming_it->id()) {
        found = true;
        break;
      }
    }

    if (!found) {
      FOR_EACH_OBSERVER(gfx::DisplayObserver, observer_list_,
                        OnDisplayRemoved(*cur_it));
    }
  }

  std::vector<gfx::Display>::const_iterator incoming_it = incoming.begin();
  for (; incoming_it != incoming.end(); ++incoming_it) {
    bool found = false;
    for (std::vector<gfx::Display>::const_iterator cur_it = displays_.begin();
         cur_it != displays_.end(); ++cur_it) {
      if (incoming_it->id() == cur_it->id()) {
        if (incoming_it->bounds() != cur_it->bounds()) {
          FOR_EACH_OBSERVER(gfx::DisplayObserver, observer_list_,
                            OnDisplayBoundsChanged(*incoming_it));
        }

        found = true;
        break;
      }
    }

    if (!found) {
      FOR_EACH_OBSERVER(gfx::DisplayObserver, observer_list_,
                        OnDisplayAdded(*incoming_it));
    }
  }

  displays_ = incoming;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenX11, gfx::Screen implementation:

bool DesktopScreenX11::IsDIPEnabled() {
  return false;
}

gfx::Point DesktopScreenX11::GetCursorScreenPoint() {
  XDisplay* display = gfx::GetXDisplay();

  ::Window root, child;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;
  XQueryPointer(display,
                DefaultRootWindow(display),
                &root,
                &child,
                &root_x,
                &root_y,
                &win_x,
                &win_y,
                &mask);

  return gfx::Point(root_x, root_y);
}

gfx::NativeWindow DesktopScreenX11::GetWindowUnderCursor() {
  return GetWindowAtScreenPoint(GetCursorScreenPoint());
}

gfx::NativeWindow DesktopScreenX11::GetWindowAtScreenPoint(
    const gfx::Point& point) {
  std::vector<aura::Window*> windows =
    DesktopRootWindowHostX11::GetAllOpenWindows();

  for (std::vector<aura::Window*>::const_iterator it = windows.begin();
       it != windows.end(); ++it) {
    if ((*it)->GetBoundsInScreen().Contains(point))
      return *it;
  }

  return NULL;
}

int DesktopScreenX11::GetNumDisplays() const {
  return displays_.size();
}

std::vector<gfx::Display> DesktopScreenX11::GetAllDisplays() const {
  return displays_;
}

gfx::Display DesktopScreenX11::GetDisplayNearestWindow(
    gfx::NativeView window) const {
  // Getting screen bounds here safely is hard.
  //
  // You'd think we'd be able to just call window->GetBoundsInScreen(), but we
  // can't because |window| (and the associated RootWindow*) can be partially
  // initialized at this point; RootWindow initializations call through into
  // GetDisplayNearestWindow(). But the X11 resources are created before we
  // create the aura::RootWindow. So we ask what the DRWHX11 believes the
  // window bounds are instead of going through the aura::Window's screen
  // bounds.
  aura::RootWindow* root_window = window->GetRootWindow();
  if (root_window) {
    DesktopRootWindowHostX11* rwh = DesktopRootWindowHostX11::GetHostForXID(
        root_window->GetAcceleratedWidget());
    if (rwh)
      return GetDisplayMatching(rwh->GetX11RootWindowBounds());
  }

  return GetPrimaryDisplay();
}

gfx::Display DesktopScreenX11::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  for (std::vector<gfx::Display>::const_iterator it = displays_.begin();
       it != displays_.end(); ++it) {
    if (it->bounds().Contains(point))
      return *it;
  }

  return GetPrimaryDisplay();
}

gfx::Display DesktopScreenX11::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  int max_area = 0;
  const gfx::Display* matching = NULL;
  for (std::vector<gfx::Display>::const_iterator it = displays_.begin();
       it != displays_.end(); ++it) {
    gfx::Rect intersect = gfx::IntersectRects(it->bounds(), match_rect);
    int area = intersect.width() * intersect.height();
    if (area > max_area) {
      max_area = area;
      matching = &*it;
    }
  }
  // Fallback to the primary display if there is no matching display.
  return matching ? *matching : GetPrimaryDisplay();
}

gfx::Display DesktopScreenX11::GetPrimaryDisplay() const {
  return displays_.front();
}

void DesktopScreenX11::AddObserver(gfx::DisplayObserver* observer) {
  observer_list_.AddObserver(observer);
}

void DesktopScreenX11::RemoveObserver(gfx::DisplayObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool DesktopScreenX11::Dispatch(const base::NativeEvent& event) {
  if (event->type - xrandr_event_base_ == RRScreenChangeNotify) {
    // Pass the event through to xlib.
    XRRUpdateConfiguration(event);
  } else if (event->type - xrandr_event_base_ == RRNotify) {
    // There's some sort of observer dispatch going on here, but I don't think
    // it's the screen's?
    DLOG(ERROR) << "DesktopScreenX11::Dispatch() -> RRNotify";

    if (configure_timer_.get()) {
      configure_timer_->Reset();
    } else {
      configure_timer_.reset(new base::OneShotTimer<DesktopScreenX11>());
      configure_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kConfigureDelayMs),
          this,
          &DesktopScreenX11::ConfigureTimerFired);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenX11, private:

DesktopScreenX11::DesktopScreenX11(
    const std::vector<gfx::Display>& test_displays)
    : xdisplay_(base::MessagePumpX11::GetDefaultXDisplay()),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      has_xrandr_(false),
      xrandr_event_base_(0),
      displays_(test_displays) {
}

std::vector<gfx::Display> DesktopScreenX11::BuildDisplaysFromXRandRInfo() {
  std::vector<gfx::Display> displays;
  XRRScreenResources* resources =
      XRRGetScreenResourcesCurrent(xdisplay_, x_root_window_);
  if (!resources) {
    LOG(ERROR) << "XRandR returned no displays. Falling back to Root Window.";
    return GetFallbackDisplayList();
  }

  bool has_work_area = false;
  gfx::Rect work_area;
  std::vector<int> value;
  if (ui::GetIntArrayProperty(x_root_window_, "_NET_WORKAREA", &value) &&
      value.size() >= 4) {
    work_area = gfx::Rect(value[0], value[1], value[2], value[3]);
    has_work_area = true;
  }

  for (int i = 0; i < resources->noutput; ++i) {
    RROutput output_id = resources->outputs[i];
    XRROutputInfo* output_info =
        XRRGetOutputInfo(xdisplay_, resources, output_id);

    bool is_connected = (output_info->connection == RR_Connected);
    if (!is_connected) {
      XRRFreeOutputInfo(output_info);
      continue;
    }

    if (output_info->crtc) {
      XRRCrtcInfo *crtc = XRRGetCrtcInfo(xdisplay_,
                                         resources,
                                         output_info->crtc);

      int64 display_id = -1;
      if (!base::GetDisplayId(output_id, i, &display_id)) {
        // It isn't ideal, but if we can't parse the EDID data, fallback on the
        // display number.
        display_id = i;
      }

      gfx::Rect crtc_bounds(crtc->x, crtc->y, crtc->width, crtc->height);
      gfx::Display display(display_id, crtc_bounds);
      if (has_work_area) {
        gfx::Rect intersection = crtc_bounds;
        intersection.Intersect(work_area);
        display.set_work_area(intersection);
      }

      displays.push_back(display);

      XRRFreeCrtcInfo(crtc);
    }

    XRRFreeOutputInfo(output_info);
  }

  XRRFreeScreenResources(resources);

  if (displays.empty())
    return GetFallbackDisplayList();

  return displays;
}

void DesktopScreenX11::ConfigureTimerFired() {
  std::vector<gfx::Display> new_displays = BuildDisplaysFromXRandRInfo();
  ProcessDisplayChange(new_displays);
}

////////////////////////////////////////////////////////////////////////////////

gfx::Screen* CreateDesktopScreen() {
  return new DesktopScreenX11;
}

}  // namespace views
