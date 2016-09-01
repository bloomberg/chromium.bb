// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationConnectionList.h"

#include "modules/EventTargetModules.h"

namespace blink {

PresentationConnectionList::PresentationConnectionList(ExecutionContext* context)
    : ContextLifecycleObserver(context)
{
}

const AtomicString& PresentationConnectionList::interfaceName() const
{
    return EventTargetNames::PresentationConnectionList;
}

ExecutionContext* PresentationConnectionList::getExecutionContext() const
{
    return ContextLifecycleObserver::getExecutionContext();
}

const HeapVector<Member<PresentationConnection>>& PresentationConnectionList::connections() const
{
    return m_connections;
}

DEFINE_TRACE(PresentationConnectionList)
{
    visitor->trace(m_connections);
    ContextLifecycleObserver::trace(visitor);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace blink
