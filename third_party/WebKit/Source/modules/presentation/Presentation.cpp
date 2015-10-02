// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/Presentation.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/EventTargetModules.h"
#include "modules/presentation/PresentationController.h"
#include "modules/presentation/PresentationRequest.h"

namespace blink {

Presentation::Presentation(LocalFrame* frame)
    : DOMWindowProperty(frame)
{
}

// static
Presentation* Presentation::create(LocalFrame* frame)
{
    ASSERT(frame);

    Presentation* presentation = new Presentation(frame);
    PresentationController* controller = PresentationController::from(*frame);
    ASSERT(controller);
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
    visitor->trace(m_connection);
    visitor->trace(m_defaultRequest);
    RefCountedGarbageCollectedEventTargetWithInlineData<Presentation>::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

PresentationRequest* Presentation::defaultRequest() const
{
    return m_defaultRequest;
}

void Presentation::setDefaultRequest(PresentationRequest* request)
{
    m_defaultRequest = request;

    if (!frame())
        return;

    PresentationController* controller = PresentationController::from(*frame());
    if (!controller)
        return;
    controller->setDefaultRequestUrl(request ? request->url() : KURL());
}

PresentationConnection* Presentation::connection() const
{
    return m_connection.get();
}

} // namespace blink
