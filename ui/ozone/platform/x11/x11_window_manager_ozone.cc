// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_window_manager_ozone.h"

#include "base/stl_util.h"
#include "ui/ozone/platform/x11/x11_window_ozone.h"

namespace ui {

X11WindowManagerOzone::X11WindowManagerOzone() : event_grabber_(nullptr) {}

X11WindowManagerOzone::~X11WindowManagerOzone() {}

void X11WindowManagerOzone::GrabEvents(X11WindowOzone* window) {
  if (event_grabber_ == window)
    return;

  X11WindowOzone* old_grabber = event_grabber_;
  if (old_grabber)
    old_grabber->OnLostCapture();

  event_grabber_ = window;
}

void X11WindowManagerOzone::UngrabEvents(X11WindowOzone* window) {
  if (event_grabber_ != window)
    return;
  event_grabber_->OnLostCapture();
  event_grabber_ = nullptr;
}

void X11WindowManagerOzone::AddWindow(X11WindowOzone* window) {
  DCHECK(window);
  DCHECK_NE(gfx::kNullAcceleratedWidget, window->widget());
  DCHECK(!base::Contains(windows_, window->widget()));
  windows_.emplace(window->widget(), window);
}

void X11WindowManagerOzone::RemoveWindow(X11WindowOzone* window) {
  DCHECK(window);
  DCHECK_NE(gfx::kNullAcceleratedWidget, window->widget());
  auto it = windows_.find(window->widget());
  DCHECK(it != windows_.end());
  windows_.erase(it);
}

X11WindowOzone* X11WindowManagerOzone::GetWindow(
    gfx::AcceleratedWidget widget) const {
  DCHECK_NE(gfx::kNullAcceleratedWidget, widget);
  auto it = windows_.find(widget);
  return it != windows_.end() ? it->second : nullptr;
}

}  // namespace ui
