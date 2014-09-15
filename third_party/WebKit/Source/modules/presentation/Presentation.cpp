// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/Presentation.h"

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
    EventTargetWithInlineData::trace(visitor);
}

} // namespace blink
