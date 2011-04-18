// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_TOUCHUI_TOUCH_FACTORY_H_
#define VIEWS_TOUCHUI_TOUCH_FACTORY_H_
#pragma once

#include <bitset>
#include <vector>

#include "base/memory/singleton.h"
#include "base/timer.h"

typedef unsigned long Cursor;
typedef unsigned long Window;
typedef struct _XDisplay Display;
typedef union _XEvent XEvent;

namespace views {

// Functions related to determining touch devices.
class TouchFactory {
 public:
  // Define the touch params following the Multi-touch Protocol.
  enum TouchParam {
    TP_TOUCH_MAJOR = 0,
    TP_TOUCH_MINOR,
    TP_ORIENTATION,
    TP_LAST_ENTRY
  };

  // Returns the TouchFactory singleton.
  static TouchFactory* GetInstance();

  // Keeps a list of touch devices so that it is possible to determine if a
  // pointer event is a touch-event or a mouse-event. The list is reset each
  // time this is called.
  void SetTouchDeviceList(const std::vector<unsigned int>& devices);

  // Is the device a touch-device?
  bool IsTouchDevice(unsigned int deviceid) const;

  // Grabs the touch devices for the specified window on the specified display.
  // Returns if grab was successful for all touch devices.
  bool GrabTouchDevices(Display* display, ::Window window);

  // Ungrabs the touch devices. Returns if ungrab was successful for all touch
  // devices.
  bool UngrabTouchDevices(Display* display);

  // Updates the root window to show (or hide) the cursor. Also indicate whether
  // the timer should be started to automatically hide the cursor after a
  // certain duration of inactivity (i.e. it is ignored if |show| is false).
  void SetCursorVisible(bool show, bool start_timer);

  // Whether the cursor is currently visible or not.
  bool is_cursor_visible() const {
    return is_cursor_visible_;
  }

  // Extract the TouchParam from the XEvent. Return true and the value is set
  // if the TouchParam is found, false and value unchanged if the TouchParam
  // is not found.
  bool ExtractTouchParam(const XEvent& xev, TouchParam tp, float* value);

 private:
  TouchFactory();

  ~TouchFactory();

  void HideCursorForInactivity() {
    SetCursorVisible(false, false);
  }

  // Setup the internal bookkeeping of the touch params valuator information for
  // touch devices
  void SetupValuator();

  // Requirement for Signleton
  friend struct DefaultSingletonTraits<TouchFactory>;

  // The default cursor is hidden after startup, and when the mouse pointer is
  // idle for a while. Once there is some event from a mouse device, the cursor
  // is immediately displayed.
  bool is_cursor_visible_;

  // The cursor is hidden if it is idle for a certain amount time. This timer
  // is used to keep track of the idleness.
  base::OneShotTimer<TouchFactory> cursor_timer_;

  // The default cursor.
  Cursor arrow_cursor_;

  // The invisible cursor.
  Cursor invisible_cursor_;

  // NOTE: To keep track of touch devices, we currently maintain a lookup table
  // to quickly decide if a device is a touch device or not. We also maintain a
  // list of the touch devices. Ideally, there will be only one touch device,
  // and instead of having the lookup table and the list, there will be a single
  // identifier for the touch device. This can be completed after enough testing
  // on real touch devices.

  static const int kMaxDeviceNum = 128;

  // A quick lookup table for determining if a device is a touch device.
  std::bitset<kMaxDeviceNum> touch_device_lookup_;

  // The list of touch devices.
  std::vector<int> touch_device_list_;

  // Index table to find the valuator for the TouchParam on the specific device
  // by valuator_lookup_[device_id][touch_params]. Use 2-D array to get fast
  // index at the expense of space. If the kMaxDeviceNum grows larger that the
  // space waste becomes a concern, the 2D lookup table can be replaced by a
  // hash map.
  char valuator_lookup_[kMaxDeviceNum][TP_LAST_ENTRY];

  DISALLOW_COPY_AND_ASSIGN(TouchFactory);
};

}  // namespace views

#endif  // VIEWS_TOUCHUI_TOUCH_FACTORY_H_
