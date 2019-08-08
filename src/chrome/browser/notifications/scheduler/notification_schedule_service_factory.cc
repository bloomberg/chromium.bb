// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/notification_schedule_service_factory.h"

#include <memory>
#include <utility>

#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler_impl.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_client_registrar.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_context.h"
#include "chrome/browser/notifications/scheduler/schedule_service_factory_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/leveldb_proto/content/proto_database_provider_factory.h"

namespace {
std::unique_ptr<notifications::NotificationSchedulerClientRegistrar>
RegisterClients() {
  auto client_registrar =
      std::make_unique<notifications::NotificationSchedulerClientRegistrar>();
  // TODO(xingliu): Register clients here.

  return client_registrar;
}

}  // namespace

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
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(leveldb_proto::ProtoDatabaseProviderFactory::GetInstance());
}

NotificationScheduleServiceFactory::~NotificationScheduleServiceFactory() =
    default;

KeyedService* NotificationScheduleServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  base::FilePath storage_dir =
      profile->GetPath().Append(chrome::kNotificationSchedulerStorageDirname);
  auto client_registrar = RegisterClients();
  auto background_task_scheduler =
      std::make_unique<NotificationBackgroundTaskSchedulerImpl>();
  auto* db_provider = leveldb_proto::ProtoDatabaseProviderFactory::GetForKey(
      profile->GetProfileKey());
  return notifications::CreateNotificationScheduleService(
      std::move(client_registrar), std::move(background_task_scheduler),
      db_provider, storage_dir);
}

content::BrowserContext*
NotificationScheduleServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Separate incognito instance that does nothing.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
