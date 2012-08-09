// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/events/event.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/base/events.h"
#include "ui/base/keycodes/keyboard_code_conversion.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/base/touch/touch_factory.h"
#include "ui/views/widget/root_view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// TouchEvent, public:

TouchEvent::TouchEvent(const base::NativeEvent& native_event)
    : LocatedEvent(native_event),
      touch_id_(ui::GetTouchId(native_event)),
      radius_x_(ui::GetTouchRadiusX(native_event)),
      radius_y_(ui::GetTouchRadiusY(native_event)),
      rotation_angle_(ui::GetTouchAngle(native_event)),
      force_(ui::GetTouchForce(native_event)) {
#if defined(USE_XI2_MT)
  if (type() == ui::ET_TOUCH_RELEASED) {
    // NOTE: The slot is allocated by TouchFactory for each XI_TouchBegin
    //       event, which carries a new tracking ID to identify a new touch
    //       sequence.
    ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
    float tracking_id;
    if (factory->ExtractTouchParam(*native_event,
                                   ui::TouchFactory::TP_TRACKING_ID,
                                   &tracking_id)) {
      factory->ReleaseSlotForTrackingID(tracking_id);
    }
  }
#else
  if (type() == ui::ET_TOUCH_PRESSED || type() == ui::ET_TOUCH_RELEASED) {
    ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
    float slot;
    if (factory->ExtractTouchParam(*native_event,
                                   ui::TouchFactory::TP_SLOT_ID, &slot)) {
      factory->SetSlotUsed(slot, type() == ui::ET_TOUCH_PRESSED);
    }
  }
#endif
}

}  // namespace views
