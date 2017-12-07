// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationManager.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/frame/Frame.h"
#include "core/frame/LocalFrame.h"
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
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

// static
NotificationManager* NotificationManager::From(
    ExecutionContext* execution_context) {
  DCHECK(execution_context);
  DCHECK(execution_context->IsContextThread());

  NotificationManager* manager = static_cast<NotificationManager*>(
      Supplement<ExecutionContext>::From(execution_context, SupplementName()));
  if (!manager) {
    manager = new NotificationManager(*execution_context);
    Supplement<ExecutionContext>::ProvideTo(*execution_context,
                                            SupplementName(), manager);
  }

  return manager;
}

// static
const char* NotificationManager::SupplementName() {
  return "NotificationManager";
}

NotificationManager::NotificationManager(ExecutionContext& execution_context)
    : Supplement<ExecutionContext>(execution_context) {}

NotificationManager::~NotificationManager() {}

mojom::blink::PermissionStatus NotificationManager::GetPermissionStatus() {
  if (GetSupplementable()->IsContextDestroyed())
    return mojom::blink::PermissionStatus::DENIED;

  mojom::blink::PermissionStatus permission_status;
  if (!GetNotificationService()->GetPermissionStatus(&permission_status)) {
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
    permission_service_.set_connection_error_handler(
        WTF::Bind(&NotificationManager::OnPermissionServiceConnectionError,
                  WrapWeakPersistent(this)));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  Document* doc = ToDocumentOrNull(context);
  permission_service_->RequestPermission(
      CreatePermissionDescriptor(mojom::blink::PermissionName::NOTIFICATIONS),
      context->GetSecurityOrigin(),
      Frame::HasTransientUserActivation(doc ? doc->GetFrame() : nullptr),
      WTF::Bind(&NotificationManager::OnPermissionRequestComplete,
                WrapPersistent(this), WrapPersistent(resolver),
                WrapPersistent(deprecated_callback)));

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

void NotificationManager::OnNotificationServiceConnectionError() {
  notification_service_.reset();
}

void NotificationManager::OnPermissionServiceConnectionError() {
  permission_service_.reset();
}

void NotificationManager::DisplayNonPersistentNotification(
    const String& title) {
  // TODO(crbug.com/595685): Pass the rest of the notification properties here.
  GetNotificationService()->DisplayNonPersistentNotification(title);
}

const mojom::blink::NotificationServicePtr&
NotificationManager::GetNotificationService() {
  if (!notification_service_) {
    if (auto* provider = GetSupplementable()->GetInterfaceProvider()) {
      provider->GetInterface(mojo::MakeRequest(&notification_service_));

      notification_service_.set_connection_error_handler(
          WTF::Bind(&NotificationManager::OnNotificationServiceConnectionError,
                    WrapWeakPersistent(this)));
    }
  }

  return notification_service_;
}

void NotificationManager::Trace(blink::Visitor* visitor) {
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
