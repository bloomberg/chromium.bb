// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_DISPATCH_CALLBACK_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_DISPATCH_CALLBACK_H_

#include "base/callback.h"

namespace ui {

class Event;

typedef base::RepeatingCallback<void(Event*)> EventDispatchCallback;

}  // namspace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_DISPATCH_CALLBACK_H_
