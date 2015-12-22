// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/permissions/Permissions.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/Nullable.h"
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
#include "wtf/NotFound.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

WebPermissionType getPermissionType(ScriptState* scriptState, const Dictionary& rawPermission, const PermissionDescriptor& permission, ExceptionState& exceptionState)
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

// Parses the raw permission dictionary and returns the PermissionType if
// parsing was successful. If an exception occurs, it will be stored in
// |exceptionState| and null will be returned. Therefore, the |exceptionState|
// should be checked before attempting to use the returned permission as the
// non-null assert will be fired otherwise.
Nullable<WebPermissionType> parsePermission(ScriptState* scriptState, const Dictionary rawPermission, ExceptionState& exceptionState)
{
    PermissionDescriptor permission = NativeValueTraits<PermissionDescriptor>::nativeValue(scriptState->isolate(), rawPermission.v8Value(), exceptionState);

    if (exceptionState.hadException()) {
        exceptionState.throwTypeError(exceptionState.message());
        return Nullable<WebPermissionType>();
    }

    WebPermissionType type = getPermissionType(scriptState, rawPermission, permission, exceptionState);
    if (exceptionState.hadException()) {
        exceptionState.throwTypeError(exceptionState.message());
        return Nullable<WebPermissionType>();
    }

    // Here we reject any permissions which are not yet supported by Blink.
    if (type == WebPermissionTypePushNotifications) {
        PushPermissionDescriptor pushPermission = NativeValueTraits<PushPermissionDescriptor>::nativeValue(scriptState->isolate(), rawPermission.v8Value(), exceptionState);
        if (exceptionState.hadException()) {
            exceptionState.throwTypeError(exceptionState.message());
            return Nullable<WebPermissionType>();
        }

        // Only "userVisibleOnly" push is supported for now.
        if (!pushPermission.userVisibleOnly()) {
            exceptionState.throwDOMException(NotSupportedError, "Push Permission without userVisibleOnly:true isn't supported yet.");
            return Nullable<WebPermissionType>();
        }
    }

    return Nullable<WebPermissionType>(type);
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

    ExceptionState exceptionState(ExceptionState::GetterContext,  "query", "Permissions", scriptState->context()->Global(), scriptState->isolate());
    Nullable<WebPermissionType> type = parsePermission(scriptState, rawPermission, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // If the current origin is a file scheme, it will unlikely return a
    // meaningful value because most APIs are broken on file scheme and no
    // permission prompt will be shown even if the returned permission will most
    // likely be "prompt".
    client->queryPermission(type.get(), KURL(KURL(), scriptState->executionContext()->securityOrigin()->toString()), new PermissionCallback(resolver, type.get()));
    return promise;
}

ScriptPromise Permissions::request(ScriptState* scriptState, const Dictionary& rawPermission)
{
    WebPermissionClient* client = getClient(scriptState->executionContext());
    if (!client)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "In its current state, the global scope can't request permissions."));

    ExceptionState exceptionState(ExceptionState::GetterContext,  "request", "Permissions", scriptState->context()->Global(), scriptState->isolate());
    Nullable<WebPermissionType> type = parsePermission(scriptState, rawPermission, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    client->requestPermission(type.get(), KURL(KURL(), scriptState->executionContext()->securityOrigin()->toString()), new PermissionCallback(resolver, type.get()));
    return promise;
}

ScriptPromise Permissions::revoke(ScriptState* scriptState, const Dictionary& rawPermission)
{
    WebPermissionClient* client = getClient(scriptState->executionContext());
    if (!client)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "In its current state, the global scope can't revoke permissions."));

    ExceptionState exceptionState(ExceptionState::GetterContext,  "revoke", "Permissions", scriptState->context()->Global(), scriptState->isolate());
    Nullable<WebPermissionType> type = parsePermission(scriptState, rawPermission, exceptionState);
    if (exceptionState.hadException())
        return exceptionState.reject(scriptState);

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    client->revokePermission(type.get(), KURL(KURL(), scriptState->executionContext()->securityOrigin()->toString()), new PermissionCallback(resolver, type.get()));
    return promise;
}

ScriptPromise Permissions::requestAll(ScriptState* scriptState, const Vector<Dictionary>& rawPermissions)
{
    WebPermissionClient* client = getClient(scriptState->executionContext());
    if (!client)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "In its current state, the global scope can't request permissions."));

    ExceptionState exceptionState(ExceptionState::GetterContext,  "request", "Permissions", scriptState->context()->Global(), scriptState->isolate());
    OwnPtr<Vector<WebPermissionType>> internalPermissions = adoptPtr(new Vector<WebPermissionType>());
    OwnPtr<Vector<int>> callerIndexToInternalIndex = adoptPtr(new Vector<int>(rawPermissions.size()));
    for (size_t i = 0; i < rawPermissions.size(); ++i) {
        const Dictionary& rawPermission = rawPermissions[i];

        Nullable<WebPermissionType> type = parsePermission(scriptState, rawPermission, exceptionState);
        if (exceptionState.hadException())
            return exceptionState.reject(scriptState);

        // Only append permissions to the vector that is passed to the client if it is not already
        // in the vector (i.e. do not duplicate permisison types).
        int internalIndex;
        auto it = internalPermissions->find(type.get());
        if (it == kNotFound) {
            internalIndex = internalPermissions->size();
            internalPermissions->append(type.get());
        } else {
            internalIndex = it;
        }
        callerIndexToInternalIndex->operator[](i) = internalIndex;
    }

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebVector<WebPermissionType> internalWebPermissions = *internalPermissions;
    client->requestPermissions(internalWebPermissions, KURL(KURL(), scriptState->executionContext()->securityOrigin()->toString()),
        new PermissionsCallback(resolver, internalPermissions.release(), callerIndexToInternalIndex.release()));
    return promise;
}

} // namespace blink
