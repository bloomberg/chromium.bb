// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_notification_manager.h"

#include <stddef.h>

#include <bitset>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/notifications/platform_notification_service_factory.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom-shared.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/android_sms/android_sms_service_factory.h"
#include "chrome/browser/chromeos/android_sms/android_sms_urls.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#endif

using content::BrowserThread;
using content::NotificationDatabaseData;
using content::PlatformNotificationContext;
using content::PushMessagingService;
using content::ServiceWorkerContext;
using content::WebContents;

namespace {
void RecordUserVisibleStatus(blink::mojom::PushUserVisibleStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.UserVisibleStatus", status);
}

content::StoragePartition* GetStoragePartition(Profile* profile,
                                               const GURL& origin) {
  return content::BrowserContext::GetStoragePartitionForSite(profile, origin);
}

NotificationDatabaseData CreateDatabaseData(
    const GURL& origin,
    int64_t service_worker_registration_id) {
  blink::PlatformNotificationData notification_data;
  notification_data.title = url_formatter::FormatUrlForSecurityDisplay(
      origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  notification_data.direction =
      blink::mojom::NotificationDirection::LEFT_TO_RIGHT;
  notification_data.body =
      l10n_util::GetStringUTF16(IDS_PUSH_MESSAGING_GENERIC_NOTIFICATION_BODY);
  notification_data.tag = kPushMessagingForcedNotificationTag;
  notification_data.icon = GURL();
  notification_data.timestamp = base::Time::Now();
  notification_data.silent = true;

  NotificationDatabaseData database_data;
  database_data.origin = origin;
  database_data.service_worker_registration_id = service_worker_registration_id;
  database_data.notification_data = notification_data;
  return database_data;
}

int CountVisibleNotifications(
    const std::vector<NotificationDatabaseData>& data) {
  if (!base::FeatureList::IsEnabled(features::kNotificationTriggers))
    return data.size();

  return std::count_if(
      data.begin(), data.end(),
      [](const NotificationDatabaseData& notification) {
        return notification.has_triggered ||
               !notification.notification_data.show_trigger_timestamp;
      });
}

}  // namespace

PushMessagingNotificationManager::PushMessagingNotificationManager(
    Profile* profile)
    : profile_(profile), budget_database_(profile) {}

PushMessagingNotificationManager::~PushMessagingNotificationManager() = default;

void PushMessagingNotificationManager::EnforceUserVisibleOnlyRequirements(
    const GURL& origin,
    int64_t service_worker_registration_id,
    EnforceRequirementsCallback message_handled_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(OS_CHROMEOS)
  if (ShouldSkipUserVisibleOnlyRequirements(origin)) {
    std::move(message_handled_callback)
        .Run(/* did_show_generic_notification= */ false);
    return;
  }
#endif

  // TODO(johnme): Relax this heuristic slightly.
  scoped_refptr<PlatformNotificationContext> notification_context =
      GetStoragePartition(profile_, origin)->GetPlatformNotificationContext();

  notification_context->ReadAllNotificationDataForServiceWorkerRegistration(
      origin, service_worker_registration_id,
      base::BindOnce(
          &PushMessagingNotificationManager::DidGetNotificationsFromDatabase,
          weak_factory_.GetWeakPtr(), origin, service_worker_registration_id,
          std::move(message_handled_callback)));
}

void PushMessagingNotificationManager::DidGetNotificationsFromDatabase(
    const GURL& origin,
    int64_t service_worker_registration_id,
    EnforceRequirementsCallback message_handled_callback,
    bool success,
    const std::vector<NotificationDatabaseData>& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(johnme): Hiding an existing notification should also count as a useful
  // user-visible action done in response to a push message - but make sure that
  // sending two messages in rapid succession which show then hide a
  // notification doesn't count.
  // TODO(knollr): Scheduling a notification should count as a user-visible
  // action, if it is not immediately cancelled or the |origin| schedules too
  // many notifications too far in the future.
  int notification_count = success ? CountVisibleNotifications(data) : 0;
  bool notification_shown = notification_count > 0;
  bool notification_needed = true;

  // Sites with a currently visible tab don't need to show notifications.
#if defined(OS_ANDROID)
  for (auto it = TabModelList::begin(); it != TabModelList::end(); ++it) {
    Profile* profile = (*it)->GetProfile();
    WebContents* active_web_contents = (*it)->GetActiveWebContents();
#else
  for (auto* browser : *BrowserList::GetInstance()) {
    Profile* profile = browser->profile();
    WebContents* active_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
#endif
    if (IsTabVisible(profile, active_web_contents, origin)) {
      notification_needed = false;
      break;
    }
  }

  // If more than one notification is showing for this Service Worker, close
  // the default notification if it happens to be part of this group.
  if (notification_count >= 2) {
    for (const auto& notification_database_data : data) {
      if (notification_database_data.notification_data.tag !=
          kPushMessagingForcedNotificationTag) {
        continue;
      }

      scoped_refptr<PlatformNotificationContext> notification_context =
          GetStoragePartition(profile_, origin)
              ->GetPlatformNotificationContext();
      notification_context->DeleteNotificationData(
          notification_database_data.notification_id, origin,
          /* close_notification= */ true, base::DoNothing());
      break;
    }
  }

  if (notification_needed && !notification_shown) {
    // If the worker needed to show a notification and didn't, see if a silent
    // push was allowed.
    budget_database_.SpendBudget(
        url::Origin::Create(origin),
        base::BindOnce(&PushMessagingNotificationManager::ProcessSilentPush,
                       weak_factory_.GetWeakPtr(), origin,
                       service_worker_registration_id,
                       std::move(message_handled_callback)));
    return;
  }

  if (notification_needed && notification_shown) {
    RecordUserVisibleStatus(
        blink::mojom::PushUserVisibleStatus::REQUIRED_AND_SHOWN);
  } else if (!notification_needed && !notification_shown) {
    RecordUserVisibleStatus(
        blink::mojom::PushUserVisibleStatus::NOT_REQUIRED_AND_NOT_SHOWN);
  } else {
    RecordUserVisibleStatus(
        blink::mojom::PushUserVisibleStatus::NOT_REQUIRED_BUT_SHOWN);
  }

  std::move(message_handled_callback)
      .Run(/* did_show_generic_notification= */ false);
}

bool PushMessagingNotificationManager::IsTabVisible(
    Profile* profile,
    WebContents* active_web_contents,
    const GURL& origin) {
  if (!active_web_contents || !active_web_contents->GetMainFrame())
    return false;

  // Don't leak information from other profiles.
  if (profile != profile_)
    return false;

  // Ignore minimized windows.
  switch (active_web_contents->GetMainFrame()->GetVisibilityState()) {
    case content::PageVisibilityState::kHidden:
    case content::PageVisibilityState::kHiddenButPainting:
      return false;
    case content::PageVisibilityState::kVisible:
      break;
  }

  // Use the visible URL since that's the one the user is aware of (and it
  // doesn't matter whether the page loaded successfully).
  GURL visible_url = active_web_contents->GetVisibleURL();

  // view-source: pages are considered to be controlled Service Worker clients
  // and thus should be considered when checking the visible URL. However, the
  // prefix has to be removed before the origins can be compared.
  if (visible_url.SchemeIs(content::kViewSourceScheme))
    visible_url = GURL(visible_url.GetContent());

  return visible_url.GetOrigin() == origin;
}

void PushMessagingNotificationManager::ProcessSilentPush(
    const GURL& origin,
    int64_t service_worker_registration_id,
    EnforceRequirementsCallback message_handled_callback,
    bool silent_push_allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the origin was allowed to issue a silent push, just return.
  if (silent_push_allowed) {
    RecordUserVisibleStatus(
        blink::mojom::PushUserVisibleStatus::REQUIRED_BUT_NOT_SHOWN_USED_GRACE);
    std::move(message_handled_callback)
        .Run(/* did_show_generic_notification= */ false);
    return;
  }

  RecordUserVisibleStatus(blink::mojom::PushUserVisibleStatus::
                              REQUIRED_BUT_NOT_SHOWN_GRACE_EXCEEDED);

  // The site failed to show a notification when one was needed, and they don't
  // have enough budget to cover the cost of suppressing, so we will show a
  // generic notification.
  NotificationDatabaseData database_data =
      CreateDatabaseData(origin, service_worker_registration_id);
  scoped_refptr<PlatformNotificationContext> notification_context =
      GetStoragePartition(profile_, origin)->GetPlatformNotificationContext();
  int64_t next_persistent_notification_id =
      PlatformNotificationServiceFactory::GetForProfile(profile_)
          ->ReadNextPersistentNotificationId();

  notification_context->WriteNotificationData(
      next_persistent_notification_id, service_worker_registration_id, origin,
      database_data,
      base::BindOnce(
          &PushMessagingNotificationManager::DidWriteNotificationData,
          weak_factory_.GetWeakPtr(), std::move(message_handled_callback)));
}

void PushMessagingNotificationManager::DidWriteNotificationData(
    EnforceRequirementsCallback message_handled_callback,
    bool success,
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success)
    DLOG(ERROR) << "Writing forced notification to database should not fail";

  std::move(message_handled_callback)
      .Run(/* did_show_generic_notification= */ true);
}

#if defined(OS_CHROMEOS)
bool PushMessagingNotificationManager::ShouldSkipUserVisibleOnlyRequirements(
    const GURL& origin) {
  // This is a short-term exception to user visible only enforcement added
  // to support for "Messages for Web" integration on ChromeOS.

  chromeos::multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client;
  if (test_multidevice_setup_client_) {
    multidevice_setup_client = test_multidevice_setup_client_;
  } else {
    multidevice_setup_client = chromeos::multidevice_setup::
        MultiDeviceSetupClientFactory::GetForProfile(profile_);
  }

  if (!multidevice_setup_client)
    return false;

  // Check if messages feature is enabled
  if (multidevice_setup_client->GetFeatureState(
          chromeos::multidevice_setup::mojom::Feature::kMessages) !=
      chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser) {
    return false;
  }

  chromeos::android_sms::AndroidSmsAppManager* android_sms_app_manager;
  if (test_android_sms_app_manager_) {
    android_sms_app_manager = test_android_sms_app_manager_;
  } else {
    chromeos::android_sms::AndroidSmsService* android_sms_service =
        chromeos::android_sms::AndroidSmsServiceFactory::GetForBrowserContext(
            profile_);
    if (!android_sms_service)
      return false;
    android_sms_app_manager = android_sms_service->android_sms_app_manager();
  }

  // Check if origin matches current messages url
  base::Optional<GURL> app_url = android_sms_app_manager->GetCurrentAppUrl();
  if (!app_url)
    app_url = chromeos::android_sms::GetAndroidMessagesURL();

  if (!origin.EqualsIgnoringRef(app_url->GetOrigin()))
    return false;

  return true;
}

void PushMessagingNotificationManager::SetTestMultiDeviceSetupClient(
    chromeos::multidevice_setup::MultiDeviceSetupClient*
        multidevice_setup_client) {
  test_multidevice_setup_client_ = multidevice_setup_client;
}

void PushMessagingNotificationManager::SetTestAndroidSmsAppManager(
    chromeos::android_sms::AndroidSmsAppManager* android_sms_app_manager) {
  test_android_sms_app_manager_ = android_sms_app_manager;
}
#endif
