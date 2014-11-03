// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/PresentationSession.h"

#include "modules/EventTargetModules.h"

namespace blink {

PresentationSession::PresentationSession(ExecutionContext* executionContext)
    : ContextLifecycleObserver(executionContext)
{
}

PresentationSession::~PresentationSession()
{
}

// static
PresentationSession* PresentationSession::create(ExecutionContext* executionContext)
{
    return new PresentationSession(executionContext);
}

const AtomicString& PresentationSession::interfaceName() const
{
    return EventTargetNames::PresentationSession;
}

ExecutionContext* PresentationSession::executionContext() const
{
    return ContextLifecycleObserver::executionContext();
}

void PresentationSession::trace(Visitor* visitor)
{
    EventTargetWithInlineData::trace(visitor);
}

void PresentationSession::postMessage(const String& message)
{
}

void PresentationSession::close()
{
}

} // namespace blink
