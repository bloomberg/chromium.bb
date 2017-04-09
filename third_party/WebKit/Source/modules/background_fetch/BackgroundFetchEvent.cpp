// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchEvent.h"

#include "modules/EventModulesNames.h"
#include "modules/background_fetch/BackgroundFetchEventInit.h"

namespace blink {

BackgroundFetchEvent::BackgroundFetchEvent(const AtomicString& type,
                                           const BackgroundFetchEventInit& init,
                                           WaitUntilObserver* observer)
    : ExtendableEvent(type, init, observer), tag_(init.tag()) {}

BackgroundFetchEvent::~BackgroundFetchEvent() = default;

String BackgroundFetchEvent::tag() const {
  return tag_;
}

const AtomicString& BackgroundFetchEvent::InterfaceName() const {
  return EventNames::BackgroundFetchEvent;
}

}  // namespace blink
