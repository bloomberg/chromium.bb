// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchFailEvent.h"

#include "modules/EventModulesNames.h"
#include "modules/background_fetch/BackgroundFetchFailEventInit.h"
#include "modules/background_fetch/BackgroundFetchSettledRequest.h"

namespace blink {

BackgroundFetchFailEvent::BackgroundFetchFailEvent(
    const AtomicString& type,
    const BackgroundFetchFailEventInit& init)
    : BackgroundFetchEvent(type, init), m_failedFetches(init.failedFetches()) {}

BackgroundFetchFailEvent::~BackgroundFetchFailEvent() = default;

HeapVector<Member<BackgroundFetchSettledRequest>>
BackgroundFetchFailEvent::failedFetches() const {
  return m_failedFetches;
}

const AtomicString& BackgroundFetchFailEvent::interfaceName() const {
  return EventNames::BackgroundFetchFailEvent;
}

DEFINE_TRACE(BackgroundFetchFailEvent) {
  visitor->trace(m_failedFetches);
  BackgroundFetchEvent::trace(visitor);
}

}  // namespace blink
