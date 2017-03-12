// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchedEvent.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "modules/EventModulesNames.h"
#include "modules/background_fetch/BackgroundFetchedEventInit.h"

namespace blink {

BackgroundFetchedEvent::BackgroundFetchedEvent(
    const AtomicString& type,
    const BackgroundFetchedEventInit& init)
    : BackgroundFetchEvent(type, init) {}

BackgroundFetchedEvent::~BackgroundFetchedEvent() = default;

ScriptPromise BackgroundFetchedEvent::updateUI(ScriptState* scriptState,
                                               String title) {
  return ScriptPromise::rejectWithDOMException(
      scriptState,
      DOMException::create(NotSupportedError, "Not implemented yet."));
}

const AtomicString& BackgroundFetchedEvent::interfaceName() const {
  return EventNames::BackgroundFetchedEvent;
}

}  // namespace blink
