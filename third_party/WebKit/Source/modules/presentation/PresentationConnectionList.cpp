// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationConnectionList.h"

#include "core/frame/UseCounter.h"
#include "modules/EventTargetModules.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationConnectionAvailableEvent.h"

namespace blink {

PresentationConnectionList::PresentationConnectionList(
    ExecutionContext* context)
    : ContextClient(context) {}

const AtomicString& PresentationConnectionList::interfaceName() const {
  return EventTargetNames::PresentationConnectionList;
}

const HeapVector<Member<PresentationConnection>>&
PresentationConnectionList::connections() const {
  return m_connections;
}

void PresentationConnectionList::addedEventListener(
    const AtomicString& eventType,
    RegisteredEventListener& registeredListener) {
  EventTargetWithInlineData::addedEventListener(eventType, registeredListener);
  if (eventType == EventTypeNames::connectionavailable)
    UseCounter::count(
        getExecutionContext(),
        UseCounter::PresentationRequestConnectionAvailableEventListener);
}

void PresentationConnectionList::addConnection(
    PresentationConnection* connection) {
  m_connections.push_back(connection);
}

void PresentationConnectionList::dispatchConnectionAvailableEvent(
    PresentationConnection* connection) {
  dispatchEvent(PresentationConnectionAvailableEvent::create(
      EventTypeNames::connectionavailable, connection));
}

bool PresentationConnectionList::isEmpty() {
  return m_connections.isEmpty();
}

DEFINE_TRACE(PresentationConnectionList) {
  visitor->trace(m_connections);
  EventTargetWithInlineData::trace(visitor);
  ContextClient::trace(visitor);
}

}  // namespace blink
