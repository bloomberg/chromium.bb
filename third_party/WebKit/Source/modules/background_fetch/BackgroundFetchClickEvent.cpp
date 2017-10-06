// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchClickEvent.h"

#include "modules/background_fetch/BackgroundFetchClickEventInit.h"
#include "modules/event_modules_names.h"

namespace blink {

BackgroundFetchClickEvent::BackgroundFetchClickEvent(
    const AtomicString& type,
    const BackgroundFetchClickEventInit& init,
    WaitUntilObserver* observer)
    : BackgroundFetchEvent(type, init, observer), state_(init.state()) {}

BackgroundFetchClickEvent::~BackgroundFetchClickEvent() = default;

AtomicString BackgroundFetchClickEvent::state() const {
  return state_;
}

const AtomicString& BackgroundFetchClickEvent::InterfaceName() const {
  return EventNames::BackgroundFetchClickEvent;
}

}  // namespace blink
