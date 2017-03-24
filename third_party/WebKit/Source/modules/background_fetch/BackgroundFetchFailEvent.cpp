// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchFailEvent.h"

#include "modules/EventModulesNames.h"
#include "modules/background_fetch/BackgroundFetchFailEventInit.h"
#include "modules/background_fetch/BackgroundFetchSettledFetch.h"
#include "modules/fetch/Request.h"
#include "modules/fetch/Response.h"
#include "public/platform/modules/background_fetch/WebBackgroundFetchSettledFetch.h"

namespace blink {

BackgroundFetchFailEvent::BackgroundFetchFailEvent(
    const AtomicString& type,
    const BackgroundFetchFailEventInit& initializer)
    : BackgroundFetchEvent(type, initializer, nullptr /* observer */),
      m_fetches(initializer.fetches()) {}

BackgroundFetchFailEvent::BackgroundFetchFailEvent(
    const AtomicString& type,
    const BackgroundFetchFailEventInit& initializer,
    const WebVector<WebBackgroundFetchSettledFetch>& fetches,
    ScriptState* scriptState,
    WaitUntilObserver* observer)
    : BackgroundFetchEvent(type, initializer, observer) {
  m_fetches.reserveInitialCapacity(fetches.size());
  for (const WebBackgroundFetchSettledFetch& fetch : fetches) {
    auto* settledFetch = BackgroundFetchSettledFetch::create(
        Request::create(scriptState, fetch.request),
        Response::create(scriptState, fetch.response));

    m_fetches.push_back(settledFetch);
  }
}

BackgroundFetchFailEvent::~BackgroundFetchFailEvent() = default;

HeapVector<Member<BackgroundFetchSettledFetch>>
BackgroundFetchFailEvent::fetches() const {
  return m_fetches;
}

const AtomicString& BackgroundFetchFailEvent::interfaceName() const {
  return EventNames::BackgroundFetchFailEvent;
}

DEFINE_TRACE(BackgroundFetchFailEvent) {
  visitor->trace(m_fetches);
  BackgroundFetchEvent::trace(visitor);
}

}  // namespace blink
