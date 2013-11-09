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

// Initializes a XEvent that holds XKeyEvent for testing. Note that ui::EF_
// flags should be passed as |flags|, not the native ones in <X11/X.h>.
EVENTS_EXPORT void InitXKeyEventForTesting(EventType type,
                                           KeyboardCode key_code,
                                           int flags,
                                           XEvent* event);
#if defined(USE_XI2_MT)
struct Valuator {
  Valuator(DeviceDataManager::DataType type, double v)
      : data_type(type), value(v) {}

  DeviceDataManager::DataType data_type;
  double value;
};

class EVENTS_EXPORT XScopedTouchEvent {
 public:
  explicit XScopedTouchEvent(XEvent* event);
  ~XScopedTouchEvent();

  operator XEvent*() { return event_.get(); }

 private:
  scoped_ptr<XEvent> event_;

  DISALLOW_COPY_AND_ASSIGN(XScopedTouchEvent);
};

EVENTS_EXPORT XEvent* CreateTouchEvent(int deviceid,
                                       int evtype,
                                       int tracking_id,
                                       const gfx::Point& location,
                                       const std::vector<Valuator>& valuators);

EVENTS_EXPORT void SetupTouchDevicesForTest(
    const std::vector<unsigned int>& devices);

#endif  // defined(USE_XI2_MT)

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

}  // namespace ui
