// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/permissions/PermissionsCallback.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/permissions/PermissionStatus.h"

namespace blink {

PermissionsCallback::PermissionsCallback(ScriptPromiseResolver* resolver, PassOwnPtr<WebVector<WebPermissionType>> permission_types)
    : m_resolver(resolver),
    m_permissionTypes(permission_types)
{
    ASSERT(m_resolver);
}

void PermissionsCallback::onSuccess(WebPassOwnPtr<WebVector<WebPermissionStatus>> permissionStatus)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
        return;

    OwnPtr<WebVector<WebPermissionStatus>> statusPtr = permissionStatus.release();

    Vector<PermissionStatus*> status;
    status.reserveCapacity(statusPtr->size());
    for (size_t i = 0; i < statusPtr->size(); ++i)
        status.append(PermissionStatus::createAndListen(m_resolver->executionContext(), (*statusPtr)[i], (*m_permissionTypes)[i]));

    m_resolver->resolve(status);
}

void PermissionsCallback::onError()
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
        return;
    m_resolver->reject();
}

} // namespace blink
