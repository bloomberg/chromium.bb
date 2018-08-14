// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_screen.h"

#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

ScenicScreen::ScenicScreen() : weak_factory_(this) {}

ScenicScreen::~ScenicScreen() = default;

void ScenicScreen::OnWindowAdded(int32_t window_id) {
  // Ensure that |window_id| is greater than the id of all other windows. This
  // allows pushing the new entry to the end of the list while keeping it
  // sorted.
  DCHECK(displays_.empty() || (window_id > displays_.back().id()));
  displays_.push_back(display::Display(window_id));

  for (auto& observer : observers_)
    observer.OnDisplayAdded(displays_.back());
}

void ScenicScreen::OnWindowRemoved(int32_t window_id) {
  auto it = FindDisplayById(window_id);
  DCHECK(it != displays_.end());
  display::Display removed_display = *it;
  displays_.erase(it);

  for (auto& observer : observers_)
    observer.OnDisplayAdded(removed_display);
}

void ScenicScreen::OnWindowMetrics(int32_t window_id,
                                   float device_pixel_ratio) {
  DisplayVector::const_iterator const_it = FindDisplayById(window_id);
  DisplayVector::iterator it =
      displays_.begin() + (const_it - displays_.begin());

  DCHECK(it != displays_.end());
  it->set_device_scale_factor(device_pixel_ratio);

  for (auto& observer : observers_) {
    observer.OnDisplayMetricsChanged(
        *it, display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR);
  }
}

base::WeakPtr<ScenicScreen> ScenicScreen::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

const std::vector<display::Display>& ScenicScreen::GetAllDisplays() const {
  return displays_;
}

display::Display ScenicScreen::GetPrimaryDisplay() const {
  // There is no primary display.
  return display::Display();
}

display::Display ScenicScreen::GetDisplayForAcceleratedWidget(
    gfx::AcceleratedWidget widget) const {
  auto display_it = FindDisplayById(widget);
  if (display_it == displays_.end()) {
    NOTREACHED();
    return display::Display();
  }

  return *display_it;
}

gfx::Point ScenicScreen::GetCursorScreenPoint() const {
  NOTIMPLEMENTED();
  return gfx::Point();
}

gfx::AcceleratedWidget ScenicScreen::GetAcceleratedWidgetAtScreenPoint(
    const gfx::Point& point) const {
  NOTIMPLEMENTED();
  return gfx::kNullAcceleratedWidget;
}

display::Display ScenicScreen::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  NOTREACHED();
  return display::Display();
}

display::Display ScenicScreen::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  NOTREACHED();
  return display::Display();
}

void ScenicScreen::AddObserver(display::DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void ScenicScreen::RemoveObserver(display::DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

ScenicScreen::DisplayVector::const_iterator ScenicScreen::FindDisplayById(
    int32_t id) const {
  DisplayVector::const_iterator r =
      std::lower_bound(displays_.begin(), displays_.end(), id,
                       [](const display::Display& display, int32_t id) {
                         return display.id() < id;
                       });
  if (r != displays_.end() && r->id() == id)
    return r;
  return displays_.end();
}

}  // namespace ui
