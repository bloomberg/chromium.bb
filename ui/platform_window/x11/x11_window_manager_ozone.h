// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_X11_WINDOW_MANAGER_OZONE_H_
#define UI_PLATFORM_WINDOW_X11_X11_WINDOW_MANAGER_OZONE_H_

#include "base/macros.h"
#include "ui/platform_window/x11/x11_window_export.h"

namespace ui {

class X11WindowOzone;

class X11_WINDOW_EXPORT X11WindowManagerOzone {
 public:
  X11WindowManagerOzone();
  ~X11WindowManagerOzone();

  // Tries to set a given X11WindowOzone as the recipient for events. It will
  // fail if there is already another X11WindowOzone as recipient.
  void GrabEvents(X11WindowOzone* window);

  // Unsets a given X11WindowOzone as the recipient for events.
  void UngrabEvents(X11WindowOzone* window);

  // Gets the current X11WindowOzone recipient of mouse events.
  X11WindowOzone* event_grabber() const { return event_grabber_; }

 private:
  X11WindowOzone* event_grabber_;

  DISALLOW_COPY_AND_ASSIGN(X11WindowManagerOzone);
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_X11_WINDOW_MANAGER_OZONE_H_
