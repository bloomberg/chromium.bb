// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushMessagingBridge.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/permissions/PermissionUtils.h"
#include "modules/push_messaging/PushError.h"
#include "modules/push_messaging/PushSubscriptionOptionsInit.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/Functional.h"
#include "public/platform/modules/push_messaging/WebPushError.h"

namespace blink {
namespace {

// Error message to explain that the userVisibleOnly flag must be set.
const char kUserVisibleOnlyRequired[] =
    "Push subscriptions that don't enable userVisibleOnly are not supported.";

String PermissionStatusToString(mojom::blink::PermissionStatus status) {
  switch (status) {
    case mojom::blink::PermissionStatus::GRANTED:
      return "granted";
    case mojom::blink::PermissionStatus::DENIED:
      return "denied";
    case mojom::blink::PermissionStatus::ASK:
      return "prompt";
  }

  NOTREACHED();
  return "denied";
}

}  // namespace

// static
PushMessagingBridge* PushMessagingBridge::From(
    ServiceWorkerRegistration* service_worker_registration) {
  DCHECK(service_worker_registration);

  PushMessagingBridge* bridge = static_cast<PushMessagingBridge*>(
      Supplement<ServiceWorkerRegistration>::From(service_worker_registration,
                                                  SupplementName()));

  if (!bridge) {
    bridge = new PushMessagingBridge(*service_worker_registration);
    Supplement<ServiceWorkerRegistration>::ProvideTo(
        *service_worker_registration, SupplementName(), bridge);
  }

  return bridge;
}

PushMessagingBridge::PushMessagingBridge(
    ServiceWorkerRegistration& registration)
    : Supplement<ServiceWorkerRegistration>(registration) {}

PushMessagingBridge::~PushMessagingBridge() = default;

const char* PushMessagingBridge::SupplementName() {
  return "PushMessagingBridge";
}

ScriptPromise PushMessagingBridge::GetPermissionState(
    ScriptState* script_state,
    const PushSubscriptionOptionsInit& options) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (!permission_service_) {
    ConnectToPermissionService(context,
                               mojo::MakeRequest(&permission_service_));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // The `userVisibleOnly` flag on |options| must be set, as it's intended to be
  // a contract with the developer that they will show a notification upon
  // receiving a push message. Permission is denied without this setting.
  //
  // TODO(peter): Would it be better to resolve DENIED rather than rejecting?
  if (!options.hasUserVisibleOnly() || !options.userVisibleOnly()) {
    resolver->Reject(
        DOMException::Create(kNotSupportedError, kUserVisibleOnlyRequired));
    return promise;
  }

  permission_service_->HasPermission(
      CreatePermissionDescriptor(mojom::blink::PermissionName::NOTIFICATIONS),
      WTF::Bind(&PushMessagingBridge::DidGetPermissionState,
                WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void PushMessagingBridge::DidGetPermissionState(
    ScriptPromiseResolver* resolver,
    mojom::blink::PermissionStatus status) {
  resolver->Resolve(PermissionStatusToString(status));
}

}  // namespace blink
