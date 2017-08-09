// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchedEvent.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/EventModulesNames.h"
#include "modules/background_fetch/BackgroundFetchBridge.h"
#include "modules/background_fetch/BackgroundFetchSettledFetch.h"
#include "modules/background_fetch/BackgroundFetchedEventInit.h"
#include "modules/fetch/Request.h"
#include "modules/fetch/Response.h"
#include "platform/bindings/ScriptState.h"
#include "public/platform/modules/background_fetch/WebBackgroundFetchSettledFetch.h"

namespace blink {

BackgroundFetchedEvent::BackgroundFetchedEvent(
    const AtomicString& type,
    const BackgroundFetchedEventInit& initializer)
    : BackgroundFetchEvent(type, initializer, nullptr /* observer */),
      fetches_(initializer.fetches()) {}

BackgroundFetchedEvent::BackgroundFetchedEvent(
    const AtomicString& type,
    const BackgroundFetchedEventInit& initializer,
    const WebVector<WebBackgroundFetchSettledFetch>& fetches,
    ScriptState* script_state,
    WaitUntilObserver* observer,
    ServiceWorkerRegistration* registration)
    : BackgroundFetchEvent(type, initializer, observer),
      registration_(registration) {
  fetches_.ReserveInitialCapacity(fetches.size());
  for (const WebBackgroundFetchSettledFetch& fetch : fetches) {
    auto* settled_fetch = BackgroundFetchSettledFetch::Create(
        Request::Create(script_state, fetch.request),
        Response::Create(script_state, fetch.response));

    fetches_.push_back(settled_fetch);
  }
}

BackgroundFetchedEvent::~BackgroundFetchedEvent() = default;

HeapVector<Member<BackgroundFetchSettledFetch>>
BackgroundFetchedEvent::fetches() const {
  return fetches_;
}

ScriptPromise BackgroundFetchedEvent::updateUI(ScriptState* script_state,
                                               const String& title) {
  if (!registration_) {
    // Return a Promise that will never settle when a developer calls this
    // method on a BackgroundFetchedEvent instance they created themselves.
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  BackgroundFetchBridge::From(registration_)
      ->UpdateUI(id(), title,
                 WTF::Bind(&BackgroundFetchedEvent::DidUpdateUI,
                           WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void BackgroundFetchedEvent::DidUpdateUI(
    ScriptPromiseResolver* resolver,
    mojom::blink::BackgroundFetchError error) {
  switch (error) {
    case mojom::blink::BackgroundFetchError::NONE:
    case mojom::blink::BackgroundFetchError::INVALID_ID:
      resolver->Resolve();
      return;
    case mojom::blink::BackgroundFetchError::DUPLICATED_ID:
    case mojom::blink::BackgroundFetchError::INVALID_ARGUMENT:
      // Not applicable for this callback.
      break;
  }

  NOTREACHED();
}

const AtomicString& BackgroundFetchedEvent::InterfaceName() const {
  return EventNames::BackgroundFetchedEvent;
}

DEFINE_TRACE(BackgroundFetchedEvent) {
  visitor->Trace(fetches_);
  visitor->Trace(registration_);
  BackgroundFetchEvent::Trace(visitor);
}

}  // namespace blink
