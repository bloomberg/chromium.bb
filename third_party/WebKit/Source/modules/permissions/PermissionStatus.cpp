// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/permissions/PermissionStatus.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/EventTargetModulesNames.h"

namespace blink {

// static
PermissionStatus* PermissionStatus::take(ScriptPromiseResolver* resolver, WebPermissionStatus* status, WebPermissionType type)
{
    PermissionStatus* permissionStatus = new PermissionStatus(resolver->executionContext(), type, *status);
    delete status;
    return permissionStatus;
}

// static
void PermissionStatus::dispose(WebPermissionStatus* status)
{
    delete status;
}

PermissionStatus::PermissionStatus(ExecutionContext* executionContext, WebPermissionType permissionType, WebPermissionStatus permissionStatus)
    : ContextLifecycleObserver(executionContext)
    , m_type(permissionType)
    , m_status(permissionStatus)
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
    switch (m_status) {
    case WebPermissionStatusGranted:
        return "granted";
    case WebPermissionStatusDenied:
        return "denied";
    case WebPermissionStatusPrompt:
        return "prompt";
    }

    ASSERT_NOT_REACHED();
    return "denied";
}

} // namespace blink
