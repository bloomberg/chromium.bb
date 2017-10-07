// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchEvent.h"

#include "modules/background_fetch/BackgroundFetchEventInit.h"
#include "modules/event_modules_names.h"

namespace blink {

BackgroundFetchEvent::BackgroundFetchEvent(const AtomicString& type,
                                           const BackgroundFetchEventInit& init,
                                           WaitUntilObserver* observer)
    : ExtendableEvent(type, init, observer), developer_id_(init.id()) {}

BackgroundFetchEvent::~BackgroundFetchEvent() = default;

String BackgroundFetchEvent::id() const {
  return developer_id_;
}

const AtomicString& BackgroundFetchEvent::InterfaceName() const {
  return EventNames::BackgroundFetchEvent;
}

}  // namespace blink
