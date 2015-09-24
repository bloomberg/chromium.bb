// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/permissions/Permissions.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/modules/v8/V8MidiPermissionDescriptor.h"
#include "bindings/modules/v8/V8PermissionDescriptor.h"
#include "bindings/modules/v8/V8PushPermissionDescriptor.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/permissions/PermissionCallback.h"
#include "modules/permissions/PermissionController.h"
#include "modules/permissions/PermissionDescriptor.h"
#include "modules/permissions/PermissionStatus.h"
#include "modules/permissions/PermissionsCallback.h"
#include "platform/Logging.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/permissions/WebPermissionClient.h"

namespace blink {

namespace {

// Here we eagerly reject any permissions that we do not support.
// If the permission is handled here, it will not be propogated up to the content layer.
// The return value indicates if the permissions has been handled by the funciton.
bool handleNotSupportedPermission(
    ScriptState* scriptState, const Dictionary& rawPermission, ScriptPromiseResolver* resolver, WebPermissionType type, TrackExceptionState& exceptionState)
{
    if (type == WebPermissionTypePushNotifications) {
        PushPermissionDescriptor pushPermission = NativeValueTraits<PushPermissionDescriptor>::nativeValue(scriptState->isolate(), rawPermission.v8Value(), exceptionState);
        // Only "userVisibleOnly" push is supported for now.
        if (!pushPermission.userVisibleOnly()) {
            resolver->reject(DOMException::create(NotSupportedError, "Push Permission without userVisibleOnly:true isn't supported yet."));
            return true;
        }
    }
    return false;
}

WebPermissionType getPermissionType(ScriptState* scriptState, const Dictionary& rawPermission, const PermissionDescriptor& permission, TrackExceptionState& exceptionState)
{
    const String& name = permission.name();
    if (name == "geolocation")
        return WebPermissionTypeGeolocation;
    if (name == "notifications")
        return WebPermissionTypeNotifications;
    if (name == "push")
        return WebPermissionTypePushNotifications;
    if (name == "midi") {
        MidiPermissionDescriptor midiPermission = NativeValueTraits<MidiPermissionDescriptor>::nativeValue(scriptState->isolate(), rawPermission.v8Value(), exceptionState);
        return midiPermission.sysex() ? WebPermissionTypeMidiSysEx : WebPermissionTypeMidi;
    }

    ASSERT_NOT_REACHED();
    return WebPermissionTypeGeolocation;
}

} // anonymous namespace

// static
WebPermissionClient* Permissions::getClient(ExecutionContext* executionContext)
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

ScriptPromise Permissions::query(ScriptState* scriptState, const Dictionary& rawPermission)
{
    WebPermissionClient* client = getClient(scriptState->executionContext());
    if (!client)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "In its current state, the global scope can't query permissions."));

    TrackExceptionState exceptionState;
    PermissionDescriptor permission = NativeValueTraits<PermissionDescriptor>::nativeValue(scriptState->isolate(), rawPermission.v8Value(), exceptionState);

    if (exceptionState.hadException())
        return ScriptPromise::reject(scriptState, v8::Exception::TypeError(v8String(scriptState->isolate(), exceptionState.message())));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebPermissionType type = getPermissionType(scriptState, rawPermission, permission, exceptionState);
    if (handleNotSupportedPermission(scriptState, rawPermission, resolver, type, exceptionState))
        return promise;

    // If the current origin is a file scheme, it will unlikely return a
    // meaningful value because most APIs are broken on file scheme and no
    // permission prompt will be shown even if the returned permission will most
    // likely be "prompt".
    client->queryPermission(type, KURL(KURL(), scriptState->executionContext()->securityOrigin()->toString()), new PermissionCallback(resolver, type));
    return promise;
}

ScriptPromise Permissions::request(ScriptState* scriptState, const Dictionary& rawPermission)
{
    WebPermissionClient* client = getClient(scriptState->executionContext());
    if (!client)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "In its current state, the global scope can't request permissions."));

    TrackExceptionState exceptionState;
    PermissionDescriptor permission = NativeValueTraits<PermissionDescriptor>::nativeValue(scriptState->isolate(), rawPermission.v8Value(), exceptionState);

    if (exceptionState.hadException())
        return ScriptPromise::reject(scriptState, v8::Exception::TypeError(v8String(scriptState->isolate(), exceptionState.message())));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebPermissionType type = getPermissionType(scriptState, rawPermission, permission, exceptionState);
    if (handleNotSupportedPermission(scriptState, rawPermission, resolver, type, exceptionState))
        return promise;

    client->requestPermission(type, KURL(KURL(), scriptState->executionContext()->securityOrigin()->toString()), new PermissionCallback(resolver, type));
    return promise;
}

ScriptPromise Permissions::request(ScriptState* scriptState, const Vector<Dictionary>& rawPermissions)
{
    WebPermissionClient* client = getClient(scriptState->executionContext());
    if (!client)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "In its current state, the global scope can't request permissions."));

    TrackExceptionState exceptionState;
    OwnPtr<WebVector<WebPermissionType>> permissions = adoptPtr(new WebVector<WebPermissionType>(rawPermissions.size()));

    for (size_t i = 0; i < rawPermissions.size(); ++i) {
        const Dictionary& rawPermission = rawPermissions[i];
        PermissionDescriptor permission = NativeValueTraits<PermissionDescriptor>::nativeValue(scriptState->isolate(), rawPermission.v8Value(), exceptionState);

        if (exceptionState.hadException())
            return ScriptPromise::reject(scriptState, v8::Exception::TypeError(v8String(scriptState->isolate(), exceptionState.message())));

        permissions->operator[](i) = getPermissionType(scriptState, rawPermission, permission, exceptionState);
    }

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // We need to do this is a separate loop because we can't create the Resolver and Promise untile we are clear of all parsing/type errors.
    for (size_t i = 0; i < rawPermissions.size(); ++i) {
        if (handleNotSupportedPermission(scriptState, rawPermissions[i], resolver, (*permissions)[i], exceptionState))
            return promise;
    }

    WebVector<WebPermissionType> permissionsCopy = *permissions;
    client->requestPermissions(permissionsCopy, KURL(KURL(), scriptState->executionContext()->securityOrigin()->toString()), new PermissionsCallback(resolver, permissions.release()));
    return promise;
}

ScriptPromise Permissions::revoke(ScriptState* scriptState, const Dictionary& rawPermission)
{
    WebPermissionClient* client = getClient(scriptState->executionContext());
    if (!client)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "In its current state, the global scope can't revoke permissions."));

    TrackExceptionState exceptionState;
    PermissionDescriptor permission = NativeValueTraits<PermissionDescriptor>::nativeValue(scriptState->isolate(), rawPermission.v8Value(), exceptionState);

    if (exceptionState.hadException())
        return ScriptPromise::reject(scriptState, v8::Exception::TypeError(v8String(scriptState->isolate(), exceptionState.message())));

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebPermissionType type = getPermissionType(scriptState, rawPermission, permission, exceptionState);
    if (handleNotSupportedPermission(scriptState, rawPermission, resolver, type, exceptionState))
        return promise;

    client->revokePermission(type, KURL(KURL(), scriptState->executionContext()->securityOrigin()->toString()), new PermissionCallback(resolver, type));
    return promise;
}

} // namespace blink
