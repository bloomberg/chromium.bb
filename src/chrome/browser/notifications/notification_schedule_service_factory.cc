// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_schedule_service_factory.h"

#include "chrome/browser/notifications/notification_background_task_scheduler_impl.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_context.h"
#include "chrome/browser/notifications/scheduler/schedule_service_factory_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
NotificationScheduleServiceFactory*
NotificationScheduleServiceFactory::GetInstance() {
  static base::NoDestructor<NotificationScheduleServiceFactory> instance;
  return instance.get();
}

// static
notifications::NotificationScheduleService*
NotificationScheduleServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<notifications::NotificationScheduleService*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

NotificationScheduleServiceFactory::NotificationScheduleServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "notifications::NotificationScheduleService",
          BrowserContextDependencyManager::GetInstance()) {}

NotificationScheduleServiceFactory::~NotificationScheduleServiceFactory() =
    default;

KeyedService* NotificationScheduleServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // Pass all dependencies to notification scheduler and build the service
  // instance.
  auto background_task_scheduler =
      std::make_unique<NotificationBackgroundTaskSchedulerImpl>();
  return notifications::CreateNotificationScheduleService(
      std::move(background_task_scheduler));
}

content::BrowserContext*
NotificationScheduleServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Separate incognito instance that does nothing.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
