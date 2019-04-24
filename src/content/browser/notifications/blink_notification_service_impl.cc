// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/blink_notification_service_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "content/browser/notifications/notification_event_dispatcher_impl.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kBadMessageImproperNotificationImage[] =
    "Received an unexpected message with image while notification images are "
    "disabled.";

// Returns the implementation of the PlatformNotificationService. May be NULL.
PlatformNotificationService* GetNotificationService() {
  return GetContentClient()->browser()->GetPlatformNotificationService();
}

bool FilterByTag(const std::string& filter_tag,
                 const NotificationDatabaseData& database_data) {
  // An empty filter tag matches all.
  if (filter_tag.empty())
    return true;
  // Otherwise we need an exact match.
  return filter_tag == database_data.notification_data.tag;
}

bool FilterByTriggered(bool include_triggered,
                       const NotificationDatabaseData& database_data) {
  // Including triggered matches all.
  if (include_triggered)
    return true;
  // Notifications without a trigger always match.
  if (!database_data.notification_data.show_trigger_timestamp)
    return true;
  // Otherwise it has to be triggered already.
  return database_data.has_triggered;
}

}  // namespace

using blink::mojom::PersistentNotificationError;

BlinkNotificationServiceImpl::BlinkNotificationServiceImpl(
    PlatformNotificationContextImpl* notification_context,
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    const url::Origin& origin,
    mojo::InterfaceRequest<blink::mojom::NotificationService> request)
    : notification_context_(notification_context),
      browser_context_(browser_context),
      service_worker_context_(std::move(service_worker_context)),
      origin_(origin),
      binding_(this, std::move(request)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(notification_context_);
  DCHECK(browser_context_);

  binding_.set_connection_error_handler(base::BindOnce(
      &BlinkNotificationServiceImpl::OnConnectionError,
      base::Unretained(this) /* the channel is owned by |this| */));
}

BlinkNotificationServiceImpl::~BlinkNotificationServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void BlinkNotificationServiceImpl::GetPermissionStatus(
    GetPermissionStatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!GetNotificationService()) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }

  std::move(callback).Run(CheckPermissionStatus());
}

void BlinkNotificationServiceImpl::OnConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  notification_context_->RemoveService(this);
  // |this| has now been deleted.
}

void BlinkNotificationServiceImpl::DisplayNonPersistentNotification(
    const std::string& token,
    const blink::PlatformNotificationData& platform_notification_data,
    const blink::NotificationResources& notification_resources,
    blink::mojom::NonPersistentNotificationListenerPtr event_listener_ptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ValidateNotificationResources(notification_resources))
    return;

  if (!GetNotificationService())
    return;

  if (CheckPermissionStatus() != blink::mojom::PermissionStatus::GRANTED)
    return;

  std::string notification_id =
      notification_context_->notification_id_generator()
          ->GenerateForNonPersistentNotification(origin_, token);

  NotificationEventDispatcherImpl* event_dispatcher =
      NotificationEventDispatcherImpl::GetInstance();
  event_dispatcher->RegisterNonPersistentNotificationListener(
      notification_id, std::move(event_listener_ptr));

  GetNotificationService()->DisplayNotification(
      browser_context_, notification_id, origin_.GetURL(),
      platform_notification_data, notification_resources);
}

void BlinkNotificationServiceImpl::CloseNonPersistentNotification(
    const std::string& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!GetNotificationService())
    return;

  if (CheckPermissionStatus() != blink::mojom::PermissionStatus::GRANTED)
    return;

  std::string notification_id =
      notification_context_->notification_id_generator()
          ->GenerateForNonPersistentNotification(origin_, token);

  GetNotificationService()->CloseNotification(browser_context_,
                                              notification_id);

  // TODO(https://crbug.com/442141): Pass a callback here to focus the tab
  // which created the notification, unless the event is canceled.
  NotificationEventDispatcherImpl::GetInstance()
      ->DispatchNonPersistentCloseEvent(notification_id, base::DoNothing());
}

blink::mojom::PermissionStatus
BlinkNotificationServiceImpl::CheckPermissionStatus() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return BrowserContext::GetPermissionController(browser_context_)
      ->GetPermissionStatus(PermissionType::NOTIFICATIONS, origin_.GetURL(),
                            origin_.GetURL());
}

bool BlinkNotificationServiceImpl::ValidateNotificationResources(
    const blink::NotificationResources& notification_resources) {
  if (notification_resources.image.drawsNothing() ||
      base::FeatureList::IsEnabled(features::kNotificationContentImage))
    return true;
  binding_.ReportBadMessage(kBadMessageImproperNotificationImage);
  // The above ReportBadMessage() closes |binding_| but does not trigger its
  // connection error handler, so we need to call the error handler explicitly
  // here to do some necessary work.
  OnConnectionError();
  return false;
}

void BlinkNotificationServiceImpl::DisplayPersistentNotification(
    int64_t service_worker_registration_id,
    const blink::PlatformNotificationData& platform_notification_data,
    const blink::NotificationResources& notification_resources,
    DisplayPersistentNotificationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!ValidateNotificationResources(notification_resources))
    return;

  if (!GetNotificationService()) {
    std::move(callback).Run(PersistentNotificationError::INTERNAL_ERROR);
    return;
  }

  if (CheckPermissionStatus() != blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(PersistentNotificationError::PERMISSION_DENIED);
    return;
  }

  int64_t next_persistent_id =
      GetNotificationService()->ReadNextPersistentNotificationId(
          browser_context_);

  NotificationDatabaseData database_data;
  database_data.origin = origin_.GetURL();
  database_data.service_worker_registration_id = service_worker_registration_id;
  database_data.notification_data = platform_notification_data;

  // TODO(https://crbug.com/870258): Validate resources are not too big (either
  // here or in the mojo struct traits).

  if (platform_notification_data.show_trigger_timestamp &&
      base::FeatureList::IsEnabled(features::kNotificationTriggers)) {
    // TODO(knollr): Let PlatformNotificationContext display all notifications,
    // even non scheduled ones and always set resources here.
    database_data.notification_resources = notification_resources;
  }

  notification_context_->WriteNotificationData(
      next_persistent_id, service_worker_registration_id, origin_.GetURL(),
      database_data,
      base::BindOnce(
          &BlinkNotificationServiceImpl::DisplayPersistentNotificationWithId,
          weak_factory_for_ui_.GetWeakPtr(), service_worker_registration_id,
          platform_notification_data, notification_resources,
          std::move(callback)));
}

void BlinkNotificationServiceImpl::DisplayPersistentNotificationWithId(
    int64_t service_worker_registration_id,
    const blink::PlatformNotificationData& platform_notification_data,
    const blink::NotificationResources& notification_resources,
    DisplayPersistentNotificationCallback callback,
    bool success,
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(PersistentNotificationError::INTERNAL_ERROR);
    return;
  }

  if (platform_notification_data.show_trigger_timestamp &&
      base::FeatureList::IsEnabled(features::kNotificationTriggers)) {
    // This notification will be handled by the |notification_context_| because
    // it has to be scheduled rather than displayed immediately.
    // TODO(knollr): Let PlatformNotificationContext display all notifications,
    // even non scheduled ones to make this code path go away.
    std::move(callback).Run(PersistentNotificationError::NONE);
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &ServiceWorkerContextWrapper::FindReadyRegistrationForId,
          service_worker_context_, service_worker_registration_id,
          origin_.GetURL(),
          base::BindOnce(
              &BlinkNotificationServiceImpl::
                  DisplayPersistentNotificationWithServiceWorkerOnIOThread,
              weak_factory_for_io_.GetWeakPtr(), notification_id,
              platform_notification_data, notification_resources,
              std::move(callback))));
}

void BlinkNotificationServiceImpl::
    DisplayPersistentNotificationWithServiceWorkerOnIOThread(
        const std::string& notification_id,
        const blink::PlatformNotificationData& platform_notification_data,
        const blink::NotificationResources& notification_resources,
        DisplayPersistentNotificationCallback callback,
        blink::ServiceWorkerStatusCode service_worker_status,
        scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PersistentNotificationError error =
      PersistentNotificationError::INTERNAL_ERROR;

  // Display the notification if the Service Worker's origin matches the origin
  // of the notification's sender.
  if (service_worker_status == blink::ServiceWorkerStatusCode::kOk &&
      registration->scope().GetOrigin() == origin_.GetURL()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &PlatformNotificationService::DisplayPersistentNotification,
            base::Unretained(GetNotificationService()), browser_context_,
            notification_id, registration->scope(), origin_.GetURL(),
            platform_notification_data, notification_resources));

    error = PersistentNotificationError::NONE;
  }

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), error));
}

void BlinkNotificationServiceImpl::ClosePersistentNotification(
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!GetNotificationService())
    return;

  if (CheckPermissionStatus() != blink::mojom::PermissionStatus::GRANTED)
    return;

  GetNotificationService()->ClosePersistentNotification(browser_context_,
                                                        notification_id);

  // Deleting the data associated with |notification_id| from the notification
  // database will be done in a task runner, but there's no reason to postpone
  // removing the notification from the user's display until that's done.
  notification_context_->DeleteNotificationData(
      notification_id, origin_.GetURL(), base::DoNothing());
}

void BlinkNotificationServiceImpl::GetNotifications(
    int64_t service_worker_registration_id,
    const std::string& filter_tag,
    bool include_triggered,
    GetNotificationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!GetNotificationService() ||
      CheckPermissionStatus() != blink::mojom::PermissionStatus::GRANTED) {
    // No permission has been granted for the given origin. It is harmless to
    // try to get notifications without permission, so return empty vectors
    // indicating that no (accessible) notifications exist at this time.
    std::move(callback).Run(std::vector<std::string>(),
                            std::vector<blink::PlatformNotificationData>());
    return;
  }

  auto read_notification_data_callback =
      base::BindOnce(&BlinkNotificationServiceImpl::DidGetNotifications,
                     weak_factory_for_ui_.GetWeakPtr(), filter_tag,
                     include_triggered, std::move(callback));

  notification_context_->ReadAllNotificationDataForServiceWorkerRegistration(
      origin_.GetURL(), service_worker_registration_id,
      std::move(read_notification_data_callback));
}

void BlinkNotificationServiceImpl::DidGetNotifications(
    const std::string& filter_tag,
    bool include_triggered,
    GetNotificationsCallback callback,
    bool success,
    const std::vector<NotificationDatabaseData>& notifications) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<std::string> ids;
  std::vector<blink::PlatformNotificationData> datas;

  for (const NotificationDatabaseData& database_data : notifications) {
    if (FilterByTag(filter_tag, database_data) &&
        FilterByTriggered(include_triggered, database_data)) {
      ids.push_back(database_data.notification_id);
      datas.push_back(database_data.notification_data);
    }
  }

  std::move(callback).Run(std::move(ids), std::move(datas));
}

}  // namespace content
