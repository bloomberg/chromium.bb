// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationManager.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "modules/notifications/Notification.h"
#include "modules/notifications/NotificationPermissionCallback.h"
#include "modules/permissions/PermissionUtils.h"
#include "platform/UserGestureIndicator.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"
#include "public/platform/modules/permissions/permission_status.mojom-blink.h"
#include "wtf/Functional.h"

namespace blink {

// static
NotificationManager* NotificationManager::from(
    ExecutionContext* executionContext) {
  DCHECK(executionContext);
  DCHECK(executionContext->isContextThread());

  NotificationManager* manager = static_cast<NotificationManager*>(
      Supplement<ExecutionContext>::from(executionContext, supplementName()));
  if (!manager) {
    manager = new NotificationManager();
    Supplement<ExecutionContext>::provideTo(*executionContext, supplementName(),
                                            manager);
  }

  return manager;
}

// static
const char* NotificationManager::supplementName() {
  return "NotificationManager";
}

NotificationManager::NotificationManager() {}

NotificationManager::~NotificationManager() {}

mojom::blink::PermissionStatus NotificationManager::permissionStatus(
    ExecutionContext* executionContext) {
  if (!m_notificationService) {
    Platform::current()->interfaceProvider()->getInterface(
        mojo::MakeRequest(&m_notificationService));
  }

  mojom::blink::PermissionStatus permissionStatus;
  const bool result = m_notificationService->GetPermissionStatus(
      executionContext->getSecurityOrigin()->toString(), &permissionStatus);
  DCHECK(result);

  return permissionStatus;
}

ScriptPromise NotificationManager::requestPermission(
    ScriptState* scriptState,
    NotificationPermissionCallback* deprecatedCallback) {
  ExecutionContext* context = scriptState->getExecutionContext();

  if (!m_permissionService) {
    connectToPermissionService(context,
                               mojo::MakeRequest(&m_permissionService));
    m_permissionService.set_connection_error_handler(convertToBaseCallback(
        WTF::bind(&NotificationManager::onPermissionServiceConnectionError,
                  wrapWeakPersistent(this))));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  m_permissionService->RequestPermission(
      createPermissionDescriptor(mojom::blink::PermissionName::NOTIFICATIONS),
      context->getSecurityOrigin(),
      UserGestureIndicator::processingUserGesture(),
      convertToBaseCallback(
          WTF::bind(&NotificationManager::onPermissionRequestComplete,
                    wrapPersistent(this), wrapPersistent(resolver),
                    wrapPersistent(deprecatedCallback))));

  return promise;
}

void NotificationManager::onPermissionRequestComplete(
    ScriptPromiseResolver* resolver,
    NotificationPermissionCallback* deprecatedCallback,
    mojom::blink::PermissionStatus status) {
  String statusString = Notification::permissionString(status);
  if (deprecatedCallback)
    deprecatedCallback->handleEvent(statusString);

  resolver->resolve(statusString);
}

void NotificationManager::onPermissionServiceConnectionError() {
  m_permissionService.reset();
}

DEFINE_TRACE(NotificationManager) {
  Supplement<ExecutionContext>::trace(visitor);
}

}  // namespace blink
