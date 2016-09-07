// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This has to be before any other includes, else default is picked up.
// See base/logging for details on this.
#define NOTIMPLEMENTED_POLICY 5

#include "ui/display/screen_base.h"

#include "ui/display/display_finder.h"

namespace display {

ScreenBase::ScreenBase() {
  display::Screen::SetScreenInstance(this);
}

ScreenBase::~ScreenBase() {
  DCHECK_EQ(this, display::Screen::GetScreen());
  display::Screen::SetScreenInstance(nullptr);
}

void ScreenBase::ProcessDisplayChanged(const display::Display& changed_display,
                                       bool is_primary) {
  if (display_list_.FindDisplayById(changed_display.id()) ==
      display_list_.displays().end()) {
    display_list_.AddDisplay(
        changed_display, is_primary ? display::DisplayList::Type::PRIMARY
                                    : display::DisplayList::Type::NOT_PRIMARY);
    return;
  }
  display_list_.UpdateDisplay(
      changed_display, is_primary ? display::DisplayList::Type::PRIMARY
                                  : display::DisplayList::Type::NOT_PRIMARY);
}

gfx::Point ScreenBase::GetCursorScreenPoint() {
  NOTIMPLEMENTED();
  return gfx::Point();
}

bool ScreenBase::IsWindowUnderCursor(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return false;
}

gfx::NativeWindow ScreenBase::GetWindowAtScreenPoint(const gfx::Point& point) {
  NOTIMPLEMENTED();
  return nullptr;
}

display::Display ScreenBase::GetPrimaryDisplay() const {
  return *display_list_.GetPrimaryDisplayIterator();
}

display::Display ScreenBase::GetDisplayNearestWindow(
    gfx::NativeView view) const {
  NOTIMPLEMENTED();
  return *display_list_.GetPrimaryDisplayIterator();
}

display::Display ScreenBase::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return *display::FindDisplayNearestPoint(display_list_.displays(), point);
}

int ScreenBase::GetNumDisplays() const {
  return static_cast<int>(display_list_.displays().size());
}

std::vector<display::Display> ScreenBase::GetAllDisplays() const {
  return display_list_.displays();
}

display::Display ScreenBase::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  const display::Display* match = display::FindDisplayWithBiggestIntersection(
      display_list_.displays(), match_rect);
  return match ? *match : GetPrimaryDisplay();
}

void ScreenBase::AddObserver(display::DisplayObserver* observer) {
  display_list_.AddObserver(observer);
}

void ScreenBase::RemoveObserver(display::DisplayObserver* observer) {
  display_list_.RemoveObserver(observer);
}

}  // namespace display
