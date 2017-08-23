// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/permissions/Permissions.h"

#include <memory>
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
#include "core/dom/ExecutionContext.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/frame/LocalFrame.h"
#include "modules/permissions/PermissionDescriptor.h"
#include "modules/permissions/PermissionStatus.h"
#include "modules/permissions/PermissionUtils.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/NotFound.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "public/platform/Platform.h"

namespace blink {

using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionService;

namespace {

// Parses the raw permission dictionary and returns the Mojo
// PermissionDescriptor if parsing was successful. If an exception occurs, it
// will be stored in |exceptionState| and null will be returned. Therefore, the
// |exceptionState| should be checked before attempting to use the returned
// permission as the non-null assert will be fired otherwise.
//
// Websites will be able to run code when `name()` is called, changing the
// current context. The caller should make sure that no assumption is made
// after this has been called.
PermissionDescriptorPtr ParsePermission(ScriptState* script_state,
                                        const Dictionary raw_permission,
                                        ExceptionState& exception_state) {
  PermissionDescriptor permission =
      NativeValueTraits<PermissionDescriptor>::NativeValue(
          script_state->GetIsolate(), raw_permission.V8Value(),
          exception_state);

  if (exception_state.HadException()) {
    exception_state.ThrowTypeError(exception_state.Message());
    return nullptr;
  }

  const String& name = permission.name();
  if (name == "geolocation")
    return CreatePermissionDescriptor(PermissionName::GEOLOCATION);
  if (name == "notifications")
    return CreatePermissionDescriptor(PermissionName::NOTIFICATIONS);
  if (name == "push") {
    PushPermissionDescriptor push_permission =
        NativeValueTraits<PushPermissionDescriptor>::NativeValue(
            script_state->GetIsolate(), raw_permission.V8Value(),
            exception_state);
    if (exception_state.HadException()) {
      exception_state.ThrowTypeError(exception_state.Message());
      return nullptr;
    }

    // Only "userVisibleOnly" push is supported for now.
    if (!push_permission.userVisibleOnly()) {
      exception_state.ThrowDOMException(
          kNotSupportedError,
          "Push Permission without userVisibleOnly:true isn't supported yet.");
      return nullptr;
    }

    return CreatePermissionDescriptor(PermissionName::PUSH_NOTIFICATIONS);
  }
  if (name == "midi") {
    MidiPermissionDescriptor midi_permission =
        NativeValueTraits<MidiPermissionDescriptor>::NativeValue(
            script_state->GetIsolate(), raw_permission.V8Value(),
            exception_state);
    return CreateMidiPermissionDescriptor(midi_permission.sysex());
  }
  if (name == "background-sync")
    return CreatePermissionDescriptor(PermissionName::BACKGROUND_SYNC);
  // TODO(riju): Remove runtime flag check when Generic Sensor feature is
  // stable.
  if (name == "ambient-light-sensor" || name == "accelerometer" ||
      name == "gyroscope" || name == "magnetometer") {
    if (!RuntimeEnabledFeatures::SensorEnabled()) {
      exception_state.ThrowTypeError("GenericSensor flag is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::SENSORS);
  }
  if (name == "accessibility-events") {
    if (!RuntimeEnabledFeatures::AccessibilityObjectModelEnabled()) {
      exception_state.ThrowTypeError(
          "Accessibility Object Model is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::ACCESSIBILITY_EVENTS);
  }

  return nullptr;
}

}  // anonymous namespace

ScriptPromise Permissions::query(ScriptState* script_state,
                                 const Dictionary& raw_permission) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kGetterContext, "Permissions",
                                 "query");
  PermissionDescriptorPtr descriptor =
      ParsePermission(script_state, raw_permission, exception_state);
  if (exception_state.HadException())
    return exception_state.Reject(script_state);

  // This must be called after `parsePermission` because the website might
  // be able to run code.
  PermissionService* service = GetService(ExecutionContext::From(script_state));
  if (!service)
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            kInvalidStateError,
            "In its current state, the global scope can't query permissions."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // If the current origin is a file scheme, it will unlikely return a
  // meaningful value because most APIs are broken on file scheme and no
  // permission prompt will be shown even if the returned permission will most
  // likely be "prompt".
  PermissionDescriptorPtr descriptor_copy = descriptor->Clone();
  service->HasPermission(
      std::move(descriptor),
      ExecutionContext::From(script_state)->GetSecurityOrigin(),
      ConvertToBaseCallback(WTF::Bind(
          &Permissions::TaskComplete, WrapPersistent(this),
          WrapPersistent(resolver), WTF::Passed(std::move(descriptor_copy)))));
  return promise;
}

ScriptPromise Permissions::request(ScriptState* script_state,
                                   const Dictionary& raw_permission) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kGetterContext, "Permissions",
                                 "request");
  PermissionDescriptorPtr descriptor =
      ParsePermission(script_state, raw_permission, exception_state);
  if (exception_state.HadException())
    return exception_state.Reject(script_state);

  // This must be called after `parsePermission` because the website might
  // be able to run code.
  PermissionService* service = GetService(ExecutionContext::From(script_state));
  if (!service)
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError,
                                           "In its current state, the global "
                                           "scope can't request permissions."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  PermissionDescriptorPtr descriptor_copy = descriptor->Clone();
  service->RequestPermission(
      std::move(descriptor),
      ExecutionContext::From(script_state)->GetSecurityOrigin(),
      UserGestureIndicator::ProcessingUserGestureThreadSafe(),
      ConvertToBaseCallback(WTF::Bind(
          &Permissions::TaskComplete, WrapPersistent(this),
          WrapPersistent(resolver), WTF::Passed(std::move(descriptor_copy)))));
  return promise;
}

ScriptPromise Permissions::revoke(ScriptState* script_state,
                                  const Dictionary& raw_permission) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kGetterContext, "Permissions",
                                 "revoke");
  PermissionDescriptorPtr descriptor =
      ParsePermission(script_state, raw_permission, exception_state);
  if (exception_state.HadException())
    return exception_state.Reject(script_state);

  // This must be called after `parsePermission` because the website might
  // be able to run code.
  PermissionService* service = GetService(ExecutionContext::From(script_state));
  if (!service)
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError,
                                           "In its current state, the global "
                                           "scope can't revoke permissions."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  PermissionDescriptorPtr descriptor_copy = descriptor->Clone();
  service->RevokePermission(
      std::move(descriptor),
      ExecutionContext::From(script_state)->GetSecurityOrigin(),
      ConvertToBaseCallback(WTF::Bind(
          &Permissions::TaskComplete, WrapPersistent(this),
          WrapPersistent(resolver), WTF::Passed(std::move(descriptor_copy)))));
  return promise;
}

ScriptPromise Permissions::requestAll(
    ScriptState* script_state,
    const Vector<Dictionary>& raw_permissions) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kGetterContext, "Permissions",
                                 "requestAll");
  Vector<PermissionDescriptorPtr> internal_permissions;
  Vector<int> caller_index_to_internal_index;
  caller_index_to_internal_index.resize(raw_permissions.size());
  for (size_t i = 0; i < raw_permissions.size(); ++i) {
    const Dictionary& raw_permission = raw_permissions[i];

    auto descriptor =
        ParsePermission(script_state, raw_permission, exception_state);
    if (exception_state.HadException())
      return exception_state.Reject(script_state);

    // Only append permissions types that are not already present in the vector.
    size_t internal_index = kNotFound;
    for (size_t j = 0; j < internal_permissions.size(); ++j) {
      if (internal_permissions[j]->name == descriptor->name) {
        internal_index = j;
        break;
      }
    }
    if (internal_index == kNotFound) {
      internal_index = internal_permissions.size();
      internal_permissions.push_back(std::move(descriptor));
    }
    caller_index_to_internal_index[i] = internal_index;
  }

  // This must be called after `parsePermission` because the website might
  // be able to run code.
  PermissionService* service = GetService(ExecutionContext::From(script_state));
  if (!service)
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(kInvalidStateError,
                                           "In its current state, the global "
                                           "scope can't request permissions."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  Vector<PermissionDescriptorPtr> internal_permissions_copy;
  internal_permissions_copy.ReserveCapacity(internal_permissions.size());
  for (const auto& descriptor : internal_permissions)
    internal_permissions_copy.push_back(descriptor->Clone());

  service->RequestPermissions(
      std::move(internal_permissions),
      ExecutionContext::From(script_state)->GetSecurityOrigin(),
      UserGestureIndicator::ProcessingUserGestureThreadSafe(),
      ConvertToBaseCallback(
          WTF::Bind(&Permissions::BatchTaskComplete, WrapPersistent(this),
                    WrapPersistent(resolver),
                    WTF::Passed(std::move(internal_permissions_copy)),
                    WTF::Passed(std::move(caller_index_to_internal_index)))));
  return promise;
}

PermissionService* Permissions::GetService(
    ExecutionContext* execution_context) {
  if (!service_ && ConnectToPermissionService(execution_context,
                                              mojo::MakeRequest(&service_)))
    service_.set_connection_error_handler(ConvertToBaseCallback(WTF::Bind(
        &Permissions::ServiceConnectionError, WrapWeakPersistent(this))));
  return service_.get();
}

void Permissions::ServiceConnectionError() {
  service_.reset();
}

void Permissions::TaskComplete(ScriptPromiseResolver* resolver,
                               mojom::blink::PermissionDescriptorPtr descriptor,
                               mojom::blink::PermissionStatus result) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;
  resolver->Resolve(
      PermissionStatus::Take(resolver, result, std::move(descriptor)));
}

void Permissions::BatchTaskComplete(
    ScriptPromiseResolver* resolver,
    Vector<mojom::blink::PermissionDescriptorPtr> descriptors,
    Vector<int> caller_index_to_internal_index,
    const Vector<mojom::blink::PermissionStatus>& results) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  // Create the response vector by finding the status for each index by
  // using the caller to internal index mapping and looking up the status
  // using the internal index obtained.
  HeapVector<Member<PermissionStatus>> result;
  result.ReserveInitialCapacity(caller_index_to_internal_index.size());
  for (int internal_index : caller_index_to_internal_index) {
    result.push_back(PermissionStatus::CreateAndListen(
        resolver->GetExecutionContext(), results[internal_index],
        descriptors[internal_index]->Clone()));
  }
  resolver->Resolve(result);
}

}  // namespace blink
