// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "ui/events/event_constants.h"
#include "ui/events/events_export.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/x/device_data_manager.h"
#include "ui/gfx/point.h"
#include "ui/gfx/x/x11_types.h"

typedef union _XEvent XEvent;

namespace ui {

struct Valuator {
  Valuator(DeviceDataManager::DataType type, double v)
      : data_type(type), value(v) {}

  DeviceDataManager::DataType data_type;
  double value;
};

class EVENTS_EXPORT ScopedXI2Event {
 public:
  explicit ScopedXI2Event(XEvent* event);
  ~ScopedXI2Event();

  operator XEvent*() { return event_.get(); }

 private:
  scoped_ptr<XEvent> event_;

  DISALLOW_COPY_AND_ASSIGN(ScopedXI2Event);
};

// Initializes a XEvent that holds XKeyEvent for testing. Note that ui::EF_
// flags should be passed as |flags|, not the native ones in <X11/X.h>.
EVENTS_EXPORT void InitXKeyEventForTesting(EventType type,
                                           KeyboardCode key_code,
                                           int flags,
                                           XEvent* event);

// Initializes a XEvent that holds XButtonEvent for testing. Note that ui::EF_
// flags should be passed as |flags|, not the native ones in <X11/X.h>.
EVENTS_EXPORT void InitXButtonEventForTesting(EventType type,
                                              int flags,
                                              XEvent* event);

// Initializes an XEvent for an Aura MouseWheelEvent. The underlying native
// event is an XButtonEvent.
EVENTS_EXPORT void InitXMouseWheelEventForTesting(int wheel_delta,
                                                  int flags,
                                                  XEvent* event);

// Creates a native scroll event, based on a XInput2Event. The caller is
// responsible for the ownership of the returned XEvent. Consider wrapping
// the XEvent in a ScopedXI2Event, as XEvent is purely a struct and simply
// deleting it will result in dangling pointers.
EVENTS_EXPORT XEvent* CreateScrollEventForTest(
    int deviceid,
    int x_offset,
    int y_offset,
    int x_offset_ordinal,
    int y_offset_ordinal,
    int finger_count);

// Creates the native XInput2 based XEvent for an aura scroll event of type
// ET_SCROLL_FLING_START or ET_SCROLL_FLING_CANCEL. The caller is responsible
// for the ownership of the returned XEvent.
EVENTS_EXPORT XEvent* CreateFlingEventForTest(
    int deviceid,
    int x_velocity,
    int y_velocity,
    int x_velocity_ordinal,
    int y_velocity_ordinal,
    bool is_cancel);

// Initializes a test touchpad device for scroll events.
EVENTS_EXPORT void SetUpScrollDeviceForTest(unsigned int deviceid);

#if defined(USE_XI2_MT)

EVENTS_EXPORT XEvent* CreateTouchEventForTest(
    int deviceid,
    int evtype,
    int tracking_id,
    const gfx::Point& location,
    const std::vector<Valuator>& valuators);

EVENTS_EXPORT void SetupTouchDevicesForTest(
    const std::vector<unsigned int>& devices);

#endif

}  // namespace ui
