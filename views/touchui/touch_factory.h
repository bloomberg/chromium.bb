// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_TOUCHUI_TOUCH_FACTORY_H_
#define VIEWS_TOUCHUI_TOUCH_FACTORY_H_
#pragma once

#include <bitset>
#include <vector>

#include "base/singleton.h"
#include "base/timer.h"

typedef unsigned long Cursor;
typedef unsigned long Window;
typedef struct _XDisplay Display;

namespace views {

// Functions related to determining touch devices.
class TouchFactory {
 public:
  // Returns the TouchFactory singleton.
  static TouchFactory* GetInstance();

  // Keep a list of touch devices so that it is possible to determine if a
  // pointer event is a touch-event or a mouse-event. The list is reset each
  // time this is called.
  void SetTouchDeviceList(const std::vector<unsigned int>& devices);

  // Is the device a touch-device?
  bool IsTouchDevice(unsigned int deviceid);

  // Grab the touch devices for the specified window on the specified display.
  // Returns if grab was successful for all touch devices.
  bool GrabTouchDevices(Display* display, ::Window window);

  // Ungrab the touch devices. Returns if ungrab was successful for all touch
  // devices.
  bool UngrabTouchDevices(Display* display);

  // Update the root window to show (or hide) the cursor. Also indicate whether
  // the timer should be started to automatically hide the cursor after a
  // certain duration of inactivity (i.e. it is ignored if |show| is false).
  void SetCursorVisible(bool show, bool start_timer);

  // Whether the cursor is currently visible or not.
  bool is_cursor_visible() {
    return is_cursor_visible_;
  }

 private:
  TouchFactory();

  ~TouchFactory();

  void HideCursorForInactivity() {
    SetCursorVisible(false, false);
  }

  // Requirement for Signleton
  friend struct DefaultSingletonTraits<TouchFactory>;

  // The default cursor is hidden after startup, and when the mouse pointer is
  // idle for a while. Once there is some event from a mouse device, the cursor
  // is immediately displayed.
  bool is_cursor_visible_;

  // The cursor is hidden if it is idle for a certain amount time. This timer
  // is used to keep track of the idleness.
  base::OneShotTimer<TouchFactory> cursor_timer_;

  // The invisible cursor.
  Cursor invisible_cursor_;

  // NOTE: To keep track of touch devices, we currently maintain a lookup table
  // to quickly decide if a device is a touch device or not. We also maintain a
  // list of the touch devices. Ideally, there will be only one touch device,
  // and instead of having the lookup table and the list, there will be a single
  // identifier for the touch device. This can be completed after enough testing
  // on real touch devices.

  // A quick lookup table for determining if a device is a touch device.
  std::bitset<128> touch_device_lookup_;

  // The list of touch devices.
  std::vector<int> touch_device_list_;

  DISALLOW_COPY_AND_ASSIGN(TouchFactory);
};

}  // namespace views

#endif  // VIEWS_TOUCHUI_TOUCH_FACTORY_H_
