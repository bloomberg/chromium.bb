// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/PresentationAvailability.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "modules/EventTargetModulesNames.h"
#include "modules/presentation/PresentationController.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"

namespace blink {

namespace {

WebPresentationClient* presentationClient(ExecutionContext* executionContext)
{
    ASSERT(executionContext && executionContext->isDocument());

    Document* document = toDocument(executionContext);
    if (!document->frame())
        return nullptr;
    PresentationController* controller = PresentationController::from(*document->frame());
    return controller ? controller->client() : nullptr;
}

} // anonymous namespace

// static
PresentationAvailability* PresentationAvailability::take(ScriptPromiseResolver* resolver, bool value)
{
    PresentationAvailability* presentationAvailability = new PresentationAvailability(resolver->executionContext(), value);
    presentationAvailability->startListening();
    return presentationAvailability;
}

PresentationAvailability::PresentationAvailability(ExecutionContext* executionContext, bool value)
    : ActiveDOMObject(executionContext)
    , m_value(value)
    , m_listening(false)
{
}

PresentationAvailability::~PresentationAvailability()
{
    stopListening();
}

const AtomicString& PresentationAvailability::interfaceName() const
{
    return EventTargetNames::PresentationAvailability;
}

ExecutionContext* PresentationAvailability::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

void PresentationAvailability::availabilityChanged(bool value)
{
    if (m_value == value)
        return;

    m_value = value;
    dispatchEvent(Event::create(EventTypeNames::change));
}

bool PresentationAvailability::hasPendingActivity() const
{
    return m_listening;
}

void PresentationAvailability::resume()
{
    startListening();
}

void PresentationAvailability::suspend()
{
    stopListening();
}

void PresentationAvailability::stop()
{
    stopListening();
}

void PresentationAvailability::startListening()
{
    ASSERT(!m_listening);

    WebPresentationClient* client = presentationClient(executionContext());
    if (!client)
        return;
    m_listening = true;
    client->startListening(this);
}

void PresentationAvailability::stopListening()
{
    if (!m_listening)
        return;

    ASSERT(executionContext());

    m_listening = false;
    WebPresentationClient* client = presentationClient(executionContext());
    if (!client)
        return;
    client->stopListening(this);
}

bool PresentationAvailability::value() const
{
    return m_value;
}

DEFINE_TRACE(PresentationAvailability)
{
    RefCountedGarbageCollectedEventTargetWithInlineData<PresentationAvailability>::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace
