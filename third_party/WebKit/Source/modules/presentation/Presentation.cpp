// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/Presentation.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"
#include "modules/presentation/AvailableChangeEvent.h"
#include "modules/presentation/PresentationController.h"
#include "modules/presentation/PresentationSessionClientCallbacks.h"

namespace blink {

Presentation::Presentation(LocalFrame* frame)
    : DOMWindowProperty(frame)
{
}

Presentation::~Presentation()
{
}

// static
Presentation* Presentation::create(LocalFrame* frame)
{
    Presentation* presentation = new Presentation(frame);
    PresentationController* controller = presentation->presentationController();
    if (controller)
        controller->setPresentation(presentation);
    return presentation;
}

const AtomicString& Presentation::interfaceName() const
{
    return EventTargetNames::Presentation;
}

ExecutionContext* Presentation::executionContext() const
{
    if (!frame())
        return nullptr;
    return frame()->document();
}

DEFINE_TRACE(Presentation)
{
    visitor->trace(m_session);
    visitor->trace(m_openSessions);
    RefCountedGarbageCollectedEventTargetWithInlineData<Presentation>::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

PresentationSession* Presentation::session() const
{
    return m_session.get();
}

ScriptPromise Presentation::startSession(ScriptState* state, const String& presentationUrl, const String& presentationId)
{
    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(state);
    ScriptPromise promise = resolver->promise();

    PresentationController* controller = presentationController();
    if (!controller) {
        resolver->reject(DOMException::create(InvalidStateError, "The object is no longer attached to the frame."));
        return promise;
    }
    controller->startSession(presentationUrl, presentationId, new PresentationSessionClientCallbacks(resolver, this));

    return promise;
}

ScriptPromise Presentation::joinSession(ScriptState* state, const String& presentationUrl, const String& presentationId)
{
    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(state);
    ScriptPromise promise = resolver->promise();

    PresentationController* controller = presentationController();
    if (!controller) {
        resolver->reject(DOMException::create(InvalidStateError, "The object is no longer attached to the frame."));
        return promise;
    }
    controller->joinSession(presentationUrl, presentationId, new PresentationSessionClientCallbacks(resolver, this));

    return promise;
}

bool Presentation::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    bool hadEventListeners = hasEventListeners(EventTypeNames::availablechange);
    if (!RefCountedGarbageCollectedEventTargetWithInlineData<Presentation>::addEventListener(eventType, listener, useCapture))
        return false;

    if (hasEventListeners(EventTypeNames::availablechange) && !hadEventListeners) {
        PresentationController* controller = presentationController();
        if (controller)
            controller->updateAvailableChangeWatched(true);
    }

    return true;
}

bool Presentation::removeEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    bool hadEventListeners = hasEventListeners(EventTypeNames::availablechange);
    if (!RefCountedGarbageCollectedEventTargetWithInlineData<Presentation>::removeEventListener(eventType, listener, useCapture))
        return false;

    if (hadEventListeners && !hasEventListeners(EventTypeNames::availablechange)) {
        PresentationController* controller = presentationController();
        if (controller)
            controller->updateAvailableChangeWatched(false);
    }

    return true;
}

void Presentation::removeAllEventListeners()
{
    bool hadEventListeners = hasEventListeners(EventTypeNames::availablechange);
    RefCountedGarbageCollectedEventTargetWithInlineData<Presentation>::removeAllEventListeners();

    if (hadEventListeners) {
        PresentationController* controller = presentationController();
        if (controller)
            controller->updateAvailableChangeWatched(false);
    }
}

void Presentation::didChangeAvailability(bool available)
{
    dispatchEvent(AvailableChangeEvent::create(EventTypeNames::availablechange, available));
}

bool Presentation::isAvailableChangeWatched() const
{
    return hasEventListeners(EventTypeNames::availablechange);
}

void Presentation::registerSession(PresentationSession* session)
{
    m_openSessions.add(session);
}

PresentationController* Presentation::presentationController()
{
    if (!frame())
        return nullptr;
    return PresentationController::from(*frame());
}

} // namespace blink
