// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/screen_mus.h"

#include "mojo/converters/geometry/geometry_type_converters.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/aura/window.h"
#include "ui/display/display_finder.h"
#include "ui/display/display_observer.h"
#include "ui/mojo/display/display_type_converters.h"
#include "ui/views/mus/screen_mus_delegate.h"
#include "ui/views/mus/window_manager_frame_values.h"

namespace mojo {

template <>
struct TypeConverter<views::WindowManagerFrameValues,
                     mus::mojom::FrameDecorationValuesPtr> {
  static views::WindowManagerFrameValues Convert(
      const mus::mojom::FrameDecorationValuesPtr& input) {
    views::WindowManagerFrameValues result;
    result.normal_insets = input->normal_client_area_insets.To<gfx::Insets>();
    result.maximized_insets =
        input->maximized_client_area_insets.To<gfx::Insets>();
    result.max_title_bar_button_width = input->max_title_bar_button_width;
    return result;
  }
};

}  // namespace mojo

namespace views {

ScreenMus::ScreenMus(ScreenMusDelegate* delegate)
    : delegate_(delegate),
      primary_display_index_(0),
      display_manager_observer_binding_(this) {
}

ScreenMus::~ScreenMus() {}

void ScreenMus::Init(shell::Connector* connector) {
  display::Screen::SetScreenInstance(this);

  connector->ConnectToInterface("mojo:mus", &display_manager_);

  display_manager_->AddObserver(
      display_manager_observer_binding_.CreateInterfacePtrAndBind());

  // We need the set of displays before we can continue. Wait for it.
  //
  // TODO(rockot): Do something better here. This should not have to block tasks
  // from running on the calling thread. http://crbug.com/594852.
  bool success = display_manager_observer_binding_.WaitForIncomingMethodCall();

  // The WaitForIncomingMethodCall() should have supplied the set of Displays,
  // unless mus is going down, in which case encountered_error() is true, or the
  // call to WaitForIncomingMethodCall() failed.
  if (displays_.empty()) {
    DCHECK(display_manager_.encountered_error() || !success);
    // In this case we install a default display and assume the process is
    // going to exit shortly so that the real value doesn't matter.
    displays_.push_back(
        display::Display(0xFFFFFFFF, gfx::Rect(0, 0, 801, 802)));
  }
}

int ScreenMus::FindDisplayIndexById(int64_t id) const {
  for (size_t i = 0; i < displays_.size(); ++i) {
    if (displays_[i].id() == id)
      return static_cast<int>(i);
  }
  return -1;
}

void ScreenMus::ProcessDisplayChanged(const display::Display& changed_display,
                                      bool is_primary) {
  const int display_index = FindDisplayIndexById(changed_display.id());
  if (display_index == -1) {
    displays_.push_back(changed_display);
    if (is_primary)
      primary_display_index_ = static_cast<int>(displays_.size()) - 1;
    FOR_EACH_OBSERVER(display::DisplayObserver, observers_,
                      OnDisplayAdded(changed_display));
    return;
  }

  display::Display* local_display = &displays_[display_index];
  uint32_t changed_values = 0;
  if (is_primary && display_index != primary_display_index_) {
    primary_display_index_ = display_index;
    // ash::DisplayManager only notifies for the Display gaining primary, not
    // the one losing it.
    changed_values |= display::DisplayObserver::DISPLAY_METRIC_PRIMARY;
  }
  if (local_display->bounds() != changed_display.bounds()) {
    local_display->set_bounds(changed_display.bounds());
    changed_values |= display::DisplayObserver::DISPLAY_METRIC_BOUNDS;
  }
  if (local_display->work_area() != changed_display.work_area()) {
    local_display->set_work_area(changed_display.work_area());
    changed_values |= display::DisplayObserver::DISPLAY_METRIC_WORK_AREA;
  }
  if (local_display->rotation() != changed_display.rotation()) {
    local_display->set_rotation(changed_display.rotation());
    changed_values |= display::DisplayObserver::DISPLAY_METRIC_ROTATION;
  }
  if (local_display->device_scale_factor() !=
      changed_display.device_scale_factor()) {
    local_display->set_device_scale_factor(
        changed_display.device_scale_factor());
    changed_values |=
        display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
  }
  FOR_EACH_OBSERVER(display::DisplayObserver, observers_,
                    OnDisplayMetricsChanged(*local_display, changed_values));
}

gfx::Point ScreenMus::GetCursorScreenPoint() {
  if (!delegate_) {
    // TODO(erg): If we need the cursor point in the window manager, we'll need
    // to make |delegate_| required. It only recently changed to be optional.
    NOTIMPLEMENTED();
    return gfx::Point();
  }

  return delegate_->GetCursorScreenPoint();
}

bool ScreenMus::IsWindowUnderCursor(gfx::NativeWindow window) {
  if (!window)
    return false;

  return window->IsVisible() &&
      window->GetBoundsInScreen().Contains(GetCursorScreenPoint());
}

gfx::NativeWindow ScreenMus::GetWindowAtScreenPoint(const gfx::Point& point) {
  NOTIMPLEMENTED();
  return nullptr;
}

display::Display ScreenMus::GetPrimaryDisplay() const {
  return displays_[primary_display_index_];
}

display::Display ScreenMus::GetDisplayNearestWindow(
    gfx::NativeView view) const {
  //NOTIMPLEMENTED();
  return GetPrimaryDisplay();
}

display::Display ScreenMus::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return *display::FindDisplayNearestPoint(displays_, point);
}

int ScreenMus::GetNumDisplays() const {
  return static_cast<int>(displays_.size());
}

std::vector<display::Display> ScreenMus::GetAllDisplays() const {
  return displays_;
}

display::Display ScreenMus::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  const display::Display* match =
      display::FindDisplayWithBiggestIntersection(displays_, match_rect);
  return match ? *match : GetPrimaryDisplay();
}

void ScreenMus::AddObserver(display::DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void ScreenMus::RemoveObserver(display::DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ScreenMus::OnDisplays(mojo::Array<mus::mojom::DisplayPtr> displays) {
  // This should only be called once from Init() before any observers have been
  // added.
  DCHECK(displays_.empty());
  displays_ = displays.To<std::vector<display::Display>>();
  DCHECK(!displays_.empty());
  for (size_t i = 0; i < displays.size(); ++i) {
    if (displays[i]->is_primary) {
      primary_display_index_ = static_cast<int>(i);
      // TODO(sky): Make WindowManagerFrameValues per display.
      WindowManagerFrameValues frame_values =
          displays[i]->frame_decoration_values.To<WindowManagerFrameValues>();
      WindowManagerFrameValues::SetInstance(frame_values);
    }
  }
}

void ScreenMus::OnDisplaysChanged(
    mojo::Array<mus::mojom::DisplayPtr> transport_displays) {
  for (size_t i = 0; i < transport_displays.size(); ++i) {
    const bool is_primary = transport_displays[i]->is_primary;
    ProcessDisplayChanged(transport_displays[i].To<display::Display>(),
                          is_primary);
    if (is_primary) {
      WindowManagerFrameValues frame_values =
          transport_displays[i]
              ->frame_decoration_values.To<WindowManagerFrameValues>();
      WindowManagerFrameValues::SetInstance(frame_values);
      if (delegate_)
        delegate_->OnWindowManagerFrameValuesChanged();
    }
  }
}

void ScreenMus::OnDisplayRemoved(int64_t id) {
  const int index = FindDisplayIndexById(id);
  DCHECK_NE(-1, index);
  // Another display must become primary before the existing primary is
  // removed.
  DCHECK_NE(index, primary_display_index_);
  const display::Display display = displays_[index];
  FOR_EACH_OBSERVER(display::DisplayObserver, observers_,
                    OnDisplayRemoved(display));
}

}  // namespace views
