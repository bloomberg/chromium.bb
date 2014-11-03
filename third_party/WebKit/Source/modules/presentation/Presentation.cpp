// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/Presentation.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "modules/EventTargetModules.h"

namespace blink {

Presentation::Presentation(ExecutionContext* executionContext)
    : ContextLifecycleObserver(executionContext)
{
}

Presentation::~Presentation()
{
}

// static
Presentation* Presentation::create(ExecutionContext* executionContext)
{
    return new Presentation(executionContext);
}

const AtomicString& Presentation::interfaceName() const
{
    return EventTargetNames::Presentation;
}

ExecutionContext* Presentation::executionContext() const
{
    return ContextLifecycleObserver::executionContext();
}

void Presentation::trace(Visitor* visitor)
{
    visitor->trace(m_session);
    EventTargetWithInlineData::trace(visitor);
}

PresentationSession* Presentation::session() const
{
    return m_session.get();
}

ScriptPromise Presentation::startSession(ScriptState* state, const String& senderId, const String& presentationId)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(state);
    ScriptPromise promise = resolver->promise();
    resolver->reject(DOMException::create(NotSupportedError, "The method is not supported yet."));
    return promise;
}

ScriptPromise Presentation::joinSession(ScriptState* state, const String& senderId, const String& presentationId)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(state);
    ScriptPromise promise = resolver->promise();
    resolver->reject(DOMException::create(NotSupportedError, "The method is not supported yet."));
    return promise;
}

} // namespace blink
