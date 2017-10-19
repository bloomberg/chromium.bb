// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationManager.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/UserGestureIndicator.h"
#include "modules/notifications/Notification.h"
#include "modules/notifications/NotificationPermissionCallback.h"
#include "modules/permissions/PermissionUtils.h"
#include "platform/bindings/ScriptState.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Functional.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"
#include "public/platform/modules/permissions/permission_status.mojom-blink.h"

namespace blink {

// static
NotificationManager* NotificationManager::From(
    ExecutionContext* execution_context) {
  DCHECK(execution_context);
  DCHECK(execution_context->IsContextThread());

  NotificationManager* manager = static_cast<NotificationManager*>(
      Supplement<ExecutionContext>::From(execution_context, SupplementName()));
  if (!manager) {
    manager = new NotificationManager();
    Supplement<ExecutionContext>::ProvideTo(*execution_context,
                                            SupplementName(), manager);
  }

  return manager;
}

// static
const char* NotificationManager::SupplementName() {
  return "NotificationManager";
}

NotificationManager::NotificationManager() {}

NotificationManager::~NotificationManager() {}

mojom::blink::PermissionStatus NotificationManager::GetPermissionStatus(
    ExecutionContext* execution_context) {
  if (!notification_service_) {
    Platform::Current()->GetInterfaceProvider()->GetInterface(
        mojo::MakeRequest(&notification_service_));
  }

  mojom::blink::PermissionStatus permission_status;
  if (!notification_service_->GetPermissionStatus(
          execution_context->GetSecurityOrigin()->ToString(),
          &permission_status)) {
    NOTREACHED();
    return mojom::blink::PermissionStatus::DENIED;
  }

  return permission_status;
}

ScriptPromise NotificationManager::RequestPermission(
    ScriptState* script_state,
    NotificationPermissionCallback* deprecated_callback) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  if (!permission_service_) {
    ConnectToPermissionService(context,
                               mojo::MakeRequest(&permission_service_));
    permission_service_.set_connection_error_handler(ConvertToBaseCallback(
        WTF::Bind(&NotificationManager::OnPermissionServiceConnectionError,
                  WrapWeakPersistent(this))));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  permission_service_->RequestPermission(
      CreatePermissionDescriptor(mojom::blink::PermissionName::NOTIFICATIONS),
      context->GetSecurityOrigin(),
      UserGestureIndicator::ProcessingUserGesture(),
      ConvertToBaseCallback(
          WTF::Bind(&NotificationManager::OnPermissionRequestComplete,
                    WrapPersistent(this), WrapPersistent(resolver),
                    WrapPersistent(deprecated_callback))));

  return promise;
}

void NotificationManager::OnPermissionRequestComplete(
    ScriptPromiseResolver* resolver,
    NotificationPermissionCallback* deprecated_callback,
    mojom::blink::PermissionStatus status) {
  String status_string = Notification::PermissionString(status);
  if (deprecated_callback)
    deprecated_callback->handleEvent(status_string);

  resolver->Resolve(status_string);
}

void NotificationManager::OnPermissionServiceConnectionError() {
  permission_service_.reset();
}

void NotificationManager::Trace(blink::Visitor* visitor) {
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
