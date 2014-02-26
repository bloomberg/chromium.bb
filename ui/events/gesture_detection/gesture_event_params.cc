// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_event_params.h"

namespace ui {

GestureEventParams::GestureEventParams(GestureEventType type,
                                       base::TimeTicks time,
                                       float x,
                                       float y,
                                       const Data& data)
    : type(type), time(time), x(x), y(y), data(data) {}

GestureEventParams::Data::Data() { memset(this, 0, sizeof(Data)); }

}  //  namespace ui
