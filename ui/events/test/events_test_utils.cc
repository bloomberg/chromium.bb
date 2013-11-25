// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/events_test_utils.h"

namespace ui {

EventTestApi::EventTestApi(Event* event) : event_(event) {}
EventTestApi::~EventTestApi() {}

LocatedEventTestApi::LocatedEventTestApi(LocatedEvent* event)
    : EventTestApi(event),
      located_event_(event) {}
LocatedEventTestApi::~LocatedEventTestApi() {}

EventTargetTestApi::EventTargetTestApi(EventTarget* target)
    : target_(target) {}

}  // namespace ui
