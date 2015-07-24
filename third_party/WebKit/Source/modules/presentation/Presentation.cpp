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

Presentation::~Presentation()
{
}

// static
Presentation* Presentation::create(LocalFrame* frame)
{
    ASSERT(frame);

    return new Presentation(frame);
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
    RefCountedGarbageCollectedEventTargetWithInlineData<Presentation>::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

PresentationRequest* Presentation::defaultRequest() const
{
    if (!frame())
        return nullptr;

    PresentationController* controller = PresentationController::from(*frame());
    return controller ? controller->defaultRequest() : nullptr;
}

void Presentation::setDefaultRequest(PresentationRequest* request)
{
    if (!frame())
        return;

    PresentationController* controller = PresentationController::from(*frame());
    if (!controller)
        return;
    controller->setDefaultRequest(request);
}

PresentationSession* Presentation::session() const
{
    return m_session.get();
}

} // namespace blink
