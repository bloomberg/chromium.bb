// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/permissions/Permissions.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/permissions/PermissionController.h"
#include "modules/permissions/PermissionQueryCallback.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/permissions/WebPermissionClient.h"

namespace blink {

namespace {

WebPermissionClient* permissionClient(ExecutionContext* executionContext)
{
    if (executionContext->isDocument()) {
        Document* document = toDocument(executionContext);
        if (!document->frame())
            return nullptr;
        PermissionController* controller = PermissionController::from(*document->frame());
        return controller ? controller->client() : nullptr;
    }
    return Platform::current()->permissionClient();
}

} // anonymous namespace

Permissions::~Permissions()
{
}

// static
ScriptPromise Permissions::query(ScriptState* scriptState, const AtomicString& permissionName)
{
    WebPermissionClient* client = permissionClient(scriptState->executionContext());
    if (!client)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "In its current state, the global scope can't query permissions."));

    WebPermissionType type;
    if (permissionName == "geolocation") {
        type = WebPermissionTypeGeolocation;
    } else if (permissionName == "notifications") {
        type = WebPermissionTypeNotifications;
    } else if (permissionName == "push-notifications") {
        type = WebPermissionTypePushNotifications;
    } else if (permissionName == "midi-sysex") {
        type = WebPermissionTypeMidiSysEx;
    } else {
        ASSERT_NOT_REACHED();
        type = WebPermissionTypeGeolocation;
    }

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // If the current origin is a file scheme, it will unlikely return a
    // meaningful value because most APIs are broken on file scheme and no
    // permission prompt will be shown even if the returned permission will most
    // likely be "prompt".
    client->queryPermission(type, KURL(KURL(), scriptState->executionContext()->securityOrigin()->toString()), new PermissionQueryCallback(resolver, type));
    return promise;
}

DEFINE_TRACE(Permissions)
{
}

} // namespace blink
