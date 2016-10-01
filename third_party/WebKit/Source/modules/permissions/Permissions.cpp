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
#include "core/frame/LocalFrame.h"
#include "modules/permissions/PermissionDescriptor.h"
#include "modules/permissions/PermissionStatus.h"
#include "platform/UserGestureIndicator.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "wtf/Functional.h"
#include "wtf/NotFound.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

using mojom::blink::PermissionName;
using mojom::blink::PermissionService;

namespace {

// Websites will be able to run code when `name()` is called, changing the
// current context. The caller should make sure that no assumption is made
// after this has been called.
PermissionName getPermissionName(ScriptState* scriptState,
                                 const Dictionary& rawPermission,
                                 const PermissionDescriptor& permission,
                                 ExceptionState& exceptionState) {
  const String& name = permission.name();
  if (name == "geolocation")
    return PermissionName::GEOLOCATION;
  if (name == "notifications")
    return PermissionName::NOTIFICATIONS;
  if (name == "push")
    return PermissionName::PUSH_NOTIFICATIONS;
  if (name == "midi") {
    MidiPermissionDescriptor midiPermission =
        NativeValueTraits<MidiPermissionDescriptor>::nativeValue(
            scriptState->isolate(), rawPermission.v8Value(), exceptionState);
    return midiPermission.sysex() ? PermissionName::MIDI_SYSEX
                                  : PermissionName::MIDI;
  }
  if (name == "background-sync")
    return PermissionName::BACKGROUND_SYNC;

  ASSERT_NOT_REACHED();
  return PermissionName::GEOLOCATION;
}

// Parses the raw permission dictionary and returns the PermissionType if
// parsing was successful. If an exception occurs, it will be stored in
// |exceptionState| and null will be returned. Therefore, the |exceptionState|
// should be checked before attempting to use the returned permission as the
// non-null assert will be fired otherwise.
Nullable<PermissionName> parsePermission(ScriptState* scriptState,
                                         const Dictionary rawPermission,
                                         ExceptionState& exceptionState) {
  PermissionDescriptor permission =
      NativeValueTraits<PermissionDescriptor>::nativeValue(
          scriptState->isolate(), rawPermission.v8Value(), exceptionState);

  if (exceptionState.hadException()) {
    exceptionState.throwTypeError(exceptionState.message());
    return Nullable<PermissionName>();
  }

  PermissionName name =
      getPermissionName(scriptState, rawPermission, permission, exceptionState);
  if (exceptionState.hadException()) {
    exceptionState.throwTypeError(exceptionState.message());
    return Nullable<PermissionName>();
  }

  // Here we reject any permissions which are not yet supported by Blink.
  if (name == PermissionName::PUSH_NOTIFICATIONS) {
    PushPermissionDescriptor pushPermission =
        NativeValueTraits<PushPermissionDescriptor>::nativeValue(
            scriptState->isolate(), rawPermission.v8Value(), exceptionState);
    if (exceptionState.hadException()) {
      exceptionState.throwTypeError(exceptionState.message());
      return Nullable<PermissionName>();
    }

    // Only "userVisibleOnly" push is supported for now.
    if (!pushPermission.userVisibleOnly()) {
      exceptionState.throwDOMException(
          NotSupportedError,
          "Push Permission without userVisibleOnly:true isn't supported yet.");
      return Nullable<PermissionName>();
    }
  }

  return Nullable<PermissionName>(name);
}

}  // anonymous namespace

// static
bool Permissions::connectToService(
    ExecutionContext* executionContext,
    mojom::blink::PermissionServiceRequest request) {
  InterfaceProvider* interfaceProvider = nullptr;
  if (executionContext->isDocument()) {
    Document* document = toDocument(executionContext);
    if (document->frame())
      interfaceProvider = document->frame()->interfaceProvider();
  } else {
    interfaceProvider = Platform::current()->interfaceProvider();
  }

  if (interfaceProvider)
    interfaceProvider->getInterface(std::move(request));
  return interfaceProvider;
}

ScriptPromise Permissions::query(ScriptState* scriptState,
                                 const Dictionary& rawPermission) {
  ExceptionState exceptionState(ExceptionState::GetterContext, "query",
                                "Permissions", scriptState->context()->Global(),
                                scriptState->isolate());
  Nullable<PermissionName> name =
      parsePermission(scriptState, rawPermission, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  // This must be called after `parsePermission` because the website might
  // be able to run code.
  PermissionService* service = getService(scriptState->getExecutionContext());
  if (!service)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidStateError,
            "In its current state, the global scope can't query permissions."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  // If the current origin is a file scheme, it will unlikely return a
  // meaningful value because most APIs are broken on file scheme and no
  // permission prompt will be shown even if the returned permission will most
  // likely be "prompt".
  service->HasPermission(
      name.get(), scriptState->getExecutionContext()->getSecurityOrigin(),
      convertToBaseCallback(WTF::bind(&Permissions::taskComplete,
                                      wrapPersistent(this),
                                      wrapPersistent(resolver), name.get())));
  return promise;
}

ScriptPromise Permissions::request(ScriptState* scriptState,
                                   const Dictionary& rawPermission) {
  ExceptionState exceptionState(ExceptionState::GetterContext, "request",
                                "Permissions", scriptState->context()->Global(),
                                scriptState->isolate());
  Nullable<PermissionName> name =
      parsePermission(scriptState, rawPermission, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  // This must be called after `parsePermission` because the website might
  // be able to run code.
  PermissionService* service = getService(scriptState->getExecutionContext());
  if (!service)
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError,
                                          "In its current state, the global "
                                          "scope can't request permissions."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  service->RequestPermission(
      name.get(), scriptState->getExecutionContext()->getSecurityOrigin(),
      UserGestureIndicator::processingUserGesture(),
      convertToBaseCallback(WTF::bind(&Permissions::taskComplete,
                                      wrapPersistent(this),
                                      wrapPersistent(resolver), name.get())));
  return promise;
}

ScriptPromise Permissions::revoke(ScriptState* scriptState,
                                  const Dictionary& rawPermission) {
  ExceptionState exceptionState(ExceptionState::GetterContext, "revoke",
                                "Permissions", scriptState->context()->Global(),
                                scriptState->isolate());
  Nullable<PermissionName> name =
      parsePermission(scriptState, rawPermission, exceptionState);
  if (exceptionState.hadException())
    return exceptionState.reject(scriptState);

  // This must be called after `parsePermission` because the website might
  // be able to run code.
  PermissionService* service = getService(scriptState->getExecutionContext());
  if (!service)
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError,
                                          "In its current state, the global "
                                          "scope can't revoke permissions."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  service->RevokePermission(
      name.get(), scriptState->getExecutionContext()->getSecurityOrigin(),
      convertToBaseCallback(WTF::bind(&Permissions::taskComplete,
                                      wrapPersistent(this),
                                      wrapPersistent(resolver), name.get())));
  return promise;
}

ScriptPromise Permissions::requestAll(
    ScriptState* scriptState,
    const Vector<Dictionary>& rawPermissions) {
  ExceptionState exceptionState(ExceptionState::GetterContext, "request",
                                "Permissions", scriptState->context()->Global(),
                                scriptState->isolate());
  Vector<PermissionName> internalPermissions;
  Vector<int> callerIndexToInternalIndex;
  callerIndexToInternalIndex.resize(rawPermissions.size());
  for (size_t i = 0; i < rawPermissions.size(); ++i) {
    const Dictionary& rawPermission = rawPermissions[i];

    Nullable<PermissionName> name =
        parsePermission(scriptState, rawPermission, exceptionState);
    if (exceptionState.hadException())
      return exceptionState.reject(scriptState);

    // Only append permissions to the vector that is passed to the client if it is not already
    // in the vector (i.e. do not duplicate permisison types).
    int internalIndex;
    auto it = internalPermissions.find(name.get());
    if (it == kNotFound) {
      internalIndex = internalPermissions.size();
      internalPermissions.append(name.get());
    } else {
      internalIndex = it;
    }
    callerIndexToInternalIndex[i] = internalIndex;
  }

  // This must be called after `parsePermission` because the website might
  // be able to run code.
  PermissionService* service = getService(scriptState->getExecutionContext());
  if (!service)
    return ScriptPromise::rejectWithDOMException(
        scriptState, DOMException::create(InvalidStateError,
                                          "In its current state, the global "
                                          "scope can't request permissions."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  service->RequestPermissions(
      internalPermissions,
      scriptState->getExecutionContext()->getSecurityOrigin(),
      UserGestureIndicator::processingUserGesture(),
      convertToBaseCallback(
          WTF::bind(&Permissions::batchTaskComplete, wrapPersistent(this),
                    wrapPersistent(resolver), internalPermissions,
                    callerIndexToInternalIndex)));
  return promise;
}

PermissionService* Permissions::getService(ExecutionContext* executionContext) {
  if (!m_service &&
      connectToService(executionContext, mojo::GetProxy(&m_service)))
    m_service.set_connection_error_handler(convertToBaseCallback(WTF::bind(
        &Permissions::serviceConnectionError, wrapWeakPersistent(this))));
  return m_service.get();
}

void Permissions::serviceConnectionError() {
  m_service.reset();
}

void Permissions::taskComplete(ScriptPromiseResolver* resolver,
                               PermissionName name,
                               mojom::blink::PermissionStatus result) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->activeDOMObjectsAreStopped())
    return;
  resolver->resolve(PermissionStatus::take(resolver, result, name));
}

void Permissions::batchTaskComplete(
    ScriptPromiseResolver* resolver,
    Vector<PermissionName> names,
    Vector<int> callerIndexToInternalIndex,
    const Vector<mojom::blink::PermissionStatus>& results) {
  if (!resolver->getExecutionContext() ||
      resolver->getExecutionContext()->activeDOMObjectsAreStopped())
    return;

  // Create the response vector by finding the status for each index by
  // using the caller to internal index mapping and looking up the status
  // using the internal index obtained.
  HeapVector<Member<PermissionStatus>> result;
  result.reserveInitialCapacity(callerIndexToInternalIndex.size());
  for (int internalIndex : callerIndexToInternalIndex)
    result.append(PermissionStatus::createAndListen(
        resolver->getExecutionContext(), results[internalIndex],
        names[internalIndex]));
  resolver->resolve(result);
}

}  // namespace blink
