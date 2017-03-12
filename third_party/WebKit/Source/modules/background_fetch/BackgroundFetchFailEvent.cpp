// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchFailEvent.h"

#include "modules/EventModulesNames.h"
#include "modules/background_fetch/BackgroundFetchFailEventInit.h"

namespace blink {

BackgroundFetchFailEvent::BackgroundFetchFailEvent(
    const AtomicString& type,
    const BackgroundFetchFailEventInit& init)
    : BackgroundFetchEvent(type, init) {}

BackgroundFetchFailEvent::~BackgroundFetchFailEvent() = default;

const AtomicString& BackgroundFetchFailEvent::interfaceName() const {
  return EventNames::BackgroundFetchFailEvent;
}

}  // namespace blink
