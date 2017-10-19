// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchFailEvent.h"

#include "modules/background_fetch/BackgroundFetchFailEventInit.h"
#include "modules/background_fetch/BackgroundFetchSettledFetch.h"
#include "modules/event_modules_names.h"
#include "modules/fetch/Request.h"
#include "modules/fetch/Response.h"
#include "public/platform/modules/background_fetch/WebBackgroundFetchSettledFetch.h"

namespace blink {

BackgroundFetchFailEvent::BackgroundFetchFailEvent(
    const AtomicString& type,
    const BackgroundFetchFailEventInit& initializer)
    : BackgroundFetchEvent(type, initializer, nullptr /* observer */),
      fetches_(initializer.fetches()) {}

BackgroundFetchFailEvent::BackgroundFetchFailEvent(
    const AtomicString& type,
    const BackgroundFetchFailEventInit& initializer,
    const WebVector<WebBackgroundFetchSettledFetch>& fetches,
    ScriptState* script_state,
    WaitUntilObserver* observer)
    : BackgroundFetchEvent(type, initializer, observer) {
  fetches_.ReserveInitialCapacity(fetches.size());
  for (const WebBackgroundFetchSettledFetch& fetch : fetches) {
    auto* settled_fetch = BackgroundFetchSettledFetch::Create(
        Request::Create(script_state, fetch.request),
        Response::Create(script_state, fetch.response));

    fetches_.push_back(settled_fetch);
  }
}

BackgroundFetchFailEvent::~BackgroundFetchFailEvent() = default;

HeapVector<Member<BackgroundFetchSettledFetch>>
BackgroundFetchFailEvent::fetches() const {
  return fetches_;
}

const AtomicString& BackgroundFetchFailEvent::InterfaceName() const {
  return EventNames::BackgroundFetchFailEvent;
}

void BackgroundFetchFailEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(fetches_);
  BackgroundFetchEvent::Trace(visitor);
}

}  // namespace blink
