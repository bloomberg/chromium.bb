// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_event_data.h"

#include "base/logging.h"

namespace ui {

GestureEventData::GestureEventData()
    : type(ET_UNKNOWN), x(0), y(0) {}

GestureEventData::GestureEventData(EventType type,
                                   base::TimeTicks time,
                                   float x,
                                   float y,
                                   const Details& details)
    : type(type), time(time), x(x), y(y), details(details) {
  DCHECK(ET_GESTURE_TYPE_FIRST <= type && type <= ET_GESTURE_TYPE_LAST);
}

GestureEventData::Details::Details() { memset(this, 0, sizeof(Details)); }

}  //  namespace ui
