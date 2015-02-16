// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/permissions/PermissionStatus.h"

#include "modules/EventTargetModulesNames.h"

namespace blink {

// static
PermissionStatus* PermissionStatus::create(ExecutionContext* executionContext)
{
    return new PermissionStatus(executionContext);
}

PermissionStatus::PermissionStatus(ExecutionContext* executionContext)
    : ContextLifecycleObserver(executionContext)
{
}

PermissionStatus::~PermissionStatus()
{
}

const AtomicString& PermissionStatus::interfaceName() const
{
    return EventTargetNames::PermissionStatus;
}

ExecutionContext* PermissionStatus::executionContext() const
{
    return ContextLifecycleObserver::executionContext();
}

DEFINE_TRACE(PermissionStatus)
{
    RefCountedGarbageCollectedEventTargetWithInlineData<PermissionStatus>::trace(visitor);
    ContextLifecycleObserver::trace(visitor);
}

String PermissionStatus::status() const
{
    // FIXME: implement.
    return String();
}

} // namespace blink
