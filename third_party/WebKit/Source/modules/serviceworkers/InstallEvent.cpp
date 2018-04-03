// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/InstallEvent.h"

#include "core/dom/ExceptionCode.h"
#include "core/execution_context/ExecutionContext.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "platform/bindings/ScriptState.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

InstallEvent* InstallEvent::Create(const AtomicString& type,
                                   const ExtendableEventInit& event_init) {
  return new InstallEvent(type, event_init);
}

InstallEvent* InstallEvent::Create(const AtomicString& type,
                                   const ExtendableEventInit& event_init,
                                   int event_id,
                                   WaitUntilObserver* observer) {
  return new InstallEvent(type, event_init, event_id, observer);
}

InstallEvent::~InstallEvent() = default;

const AtomicString& InstallEvent::InterfaceName() const {
  return EventNames::InstallEvent;
}

InstallEvent::InstallEvent(const AtomicString& type,
                           const ExtendableEventInit& initializer)
    : ExtendableEvent(type, initializer), event_id_(0) {}

InstallEvent::InstallEvent(const AtomicString& type,
                           const ExtendableEventInit& initializer,
                           int event_id,
                           WaitUntilObserver* observer)
    : ExtendableEvent(type, initializer, observer), event_id_(event_id) {}

}  // namespace blink
