// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/PresentationSession.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"
#include "modules/presentation/Presentation.h"
#include "modules/presentation/PresentationController.h"
#include "public/platform/WebString.h"
#include "public/platform/modules/presentation/WebPresentationSessionClient.h"
#include "wtf/OwnPtr.h"

namespace blink {

PresentationSession::PresentationSession(LocalFrame* frame, const WebString& id, const WebString& url)
    : DOMWindowProperty(frame)
    , m_id(id)
    , m_url(url)
    , m_state("disconnected")
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

void PresentationSession::postMessage(const String& message)
{
}

void PresentationSession::close()
{
    if (m_state != "connected")
        return;
    PresentationController* controller = presentationController();
    if (controller)
        controller->closeSession(m_url, m_id);
}

PresentationController* PresentationSession::presentationController()
{
    if (!frame())
        return nullptr;
    return PresentationController::from(*frame());
}

} // namespace blink
