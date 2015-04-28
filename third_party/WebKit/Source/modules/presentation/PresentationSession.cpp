// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/PresentationSession.h"

#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/events/MessageEvent.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"
#include "modules/presentation/Presentation.h"
#include "modules/presentation/PresentationController.h"
#include "public/platform/modules/presentation/WebPresentationSessionClient.h"
#include "wtf/Assertions.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/AtomicString.h"

namespace blink {

namespace {

const AtomicString& SessionStateToString(WebPresentationSessionState state)
{
    DEFINE_STATIC_LOCAL(const AtomicString, connectedValue, ("connected", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, disconnectedValue, ("disconnected", AtomicString::ConstructFromLiteral));

    switch (state) {
    case WebPresentationSessionState::Connected:
        return connectedValue;
    case WebPresentationSessionState::Disconnected:
        return disconnectedValue;
    }

    ASSERT_NOT_REACHED();
    return disconnectedValue;
}

void throwPresentationDisconnectedError(ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(InvalidStateError, "Presentation session is disconnected.");
}

} // namespace

PresentationSession::PresentationSession(LocalFrame* frame, const String& id, const String& url)
    : DOMWindowProperty(frame)
    , m_id(id)
    , m_url(url)
    , m_state(WebPresentationSessionState::Disconnected)
{
}

PresentationSession::~PresentationSession()
{
}

// static
PresentationSession* PresentationSession::take(WebPresentationSessionClient* clientRaw, Presentation* presentation)
{
    ASSERT(clientRaw);
    ASSERT(presentation);
    OwnPtr<WebPresentationSessionClient> client = adoptPtr(clientRaw);

    PresentationSession* session = new PresentationSession(presentation->frame(), client->getId(), client->getUrl());
    presentation->registerSession(session);
    return session;
}

// static
void PresentationSession::dispose(WebPresentationSessionClient* client)
{
    delete client;
}

const AtomicString& PresentationSession::interfaceName() const
{
    return EventTargetNames::PresentationSession;
}

ExecutionContext* PresentationSession::executionContext() const
{
    if (!frame())
        return nullptr;
    return frame()->document();}

DEFINE_TRACE(PresentationSession)
{
    RefCountedGarbageCollectedEventTargetWithInlineData<PresentationSession>::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

const AtomicString& PresentationSession::state() const
{
    return SessionStateToString(m_state);
}

void PresentationSession::send(const String& message, ExceptionState& exceptionState)
{
    if (m_state == WebPresentationSessionState::Disconnected) {
        throwPresentationDisconnectedError(exceptionState);
        return;
    }

    PresentationController* controller = presentationController();
    if (controller)
        controller->send(m_url, m_id, message);
}

void PresentationSession::send(PassRefPtr<DOMArrayBuffer> data, ExceptionState& exceptionState)
{
    ASSERT(data && data->buffer());
    sendInternal(static_cast<const uint8_t*>(data->data()), data->byteLength(), exceptionState);
}

void PresentationSession::send(PassRefPtr<DOMArrayBufferView> data, ExceptionState& exceptionState)
{
    ASSERT(data);
    sendInternal(static_cast<const uint8_t*>(data->baseAddress()), data->byteLength(), exceptionState);
}

void PresentationSession::sendInternal(const uint8_t* data, size_t size, ExceptionState& exceptionState)
{
    ASSERT(data);
    if (m_state == WebPresentationSessionState::Disconnected) {
        throwPresentationDisconnectedError(exceptionState);
        return;
    }

    PresentationController* controller = presentationController();
    if (controller)
        controller->send(m_url, m_id, data, size);
}

void PresentationSession::didReceiveTextMessage(const String& message)
{
    dispatchEvent(MessageEvent::create(message));
}

void PresentationSession::close()
{
    if (m_state != WebPresentationSessionState::Connected)
        return;
    PresentationController* controller = presentationController();
    if (controller)
        controller->closeSession(m_url, m_id);
}

bool PresentationSession::matches(WebPresentationSessionClient* client) const
{
    return client && m_url == static_cast<String>(client->getUrl()) && m_id == static_cast<String>(client->getId());
}

void PresentationSession::didChangeState(WebPresentationSessionState state)
{
    if (m_state == state)
        return;

    m_state = state;
    dispatchEvent(Event::create(EventTypeNames::statechange));
}

PresentationController* PresentationSession::presentationController()
{
    if (!frame())
        return nullptr;
    return PresentationController::from(*frame());
}

} // namespace blink
