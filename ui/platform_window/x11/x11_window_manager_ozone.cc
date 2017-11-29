// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_window_manager_ozone.h"

namespace ui {

X11WindowManagerOzone::X11WindowManagerOzone() : event_grabber_(nullptr) {}

X11WindowManagerOzone::~X11WindowManagerOzone() {}

void X11WindowManagerOzone::GrabEvents(X11WindowOzone* window) {
  if (event_grabber_ != window)
    return;
  event_grabber_ = window;
}

void X11WindowManagerOzone::UngrabEvents(X11WindowOzone* window) {
  if (event_grabber_ != window)
    return;
  event_grabber_ = nullptr;
}

}  // namespace ui
