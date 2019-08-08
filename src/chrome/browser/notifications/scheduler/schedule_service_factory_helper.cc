// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/schedule_service_factory_helper.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/browser/notifications/scheduler/display_decider.h"
#include "chrome/browser/notifications/scheduler/icon_store.h"
#include "chrome/browser/notifications/scheduler/impression_history_tracker.h"
#include "chrome/browser/notifications/scheduler/impression_store.h"
#include "chrome/browser/notifications/scheduler/init_aware_scheduler.h"
#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/notification_schedule_service_impl.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_client_registrar.h"
#include "chrome/browser/notifications/scheduler/notification_scheduler_context.h"
#include "chrome/browser/notifications/scheduler/notification_store.h"
#include "chrome/browser/notifications/scheduler/scheduled_notification_manager.h"
#include "chrome/browser/notifications/scheduler/scheduler_config.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

namespace notifications {
namespace {
const base::FilePath::CharType kImpressionDBName[] =
    FILE_PATH_LITERAL("ImpressionDB");
const base::FilePath::CharType kIconDBName[] = FILE_PATH_LITERAL("IconDB");
const base::FilePath::CharType kNotificationDBName[] =
    FILE_PATH_LITERAL("NotificationDB");
}  // namespace

KeyedService* CreateNotificationScheduleService(
    std::unique_ptr<NotificationSchedulerClientRegistrar> client_registrar,
    std::unique_ptr<NotificationBackgroundTaskScheduler>
        background_task_scheduler,
    leveldb_proto::ProtoDatabaseProvider* db_provider,
    const base::FilePath& storage_dir) {
  auto config = SchedulerConfig::Create();
  auto task_runner = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  // Build icon store.
  base::FilePath icon_store_dir = storage_dir.Append(kIconDBName);
  auto icon_db = db_provider->GetDB<proto::Icon, IconEntry>(
      leveldb_proto::ProtoDbType::NOTIFICATION_SCHEDULER_ICON_STORE,
      icon_store_dir, task_runner);
  auto icon_store = std::make_unique<IconProtoDbStore>(std::move(icon_db));

  // Build impression store.
  base::FilePath impression_store_dir = storage_dir.Append(kImpressionDBName);
  auto impression_db = db_provider->GetDB<proto::ClientState, ClientState>(
      leveldb_proto::ProtoDbType::NOTIFICATION_SCHEDULER_IMPRESSION_STORE,
      impression_store_dir, task_runner);
  auto impression_store =
      std::make_unique<ImpressionStore>(std::move(impression_db));
  auto impression_tracker = std::make_unique<ImpressionHistoryTrackerImpl>(
      *config.get(), std::move(impression_store));

  // Build notification store.
  base::FilePath notification_store_dir =
      storage_dir.Append(kNotificationDBName);
  auto notification_db = db_provider->GetDB<proto::NotificationEntry,
                                            notifications::NotificationEntry>(
      leveldb_proto::ProtoDbType::NOTIFICATION_SCHEDULER_NOTIFICATION_STORE,
      notification_store_dir, task_runner);
  auto notification_store =
      std::make_unique<NotificationStore>(std::move(notification_db));

  std::unique_ptr<ScheduledNotificationManager> notification_manager;
  notification_manager->Create(std::move(notification_store));

  auto context = std::make_unique<NotificationSchedulerContext>(
      std::move(client_registrar), std::move(background_task_scheduler),
      std::move(icon_store), std::move(impression_tracker),
      std::move(notification_manager), DisplayDecider::Create(),
      std::move(config));

  auto scheduler = NotificationScheduler::Create(std::move(context));
  auto init_aware_scheduler =
      std::make_unique<InitAwareNotificationScheduler>(std::move(scheduler));
  return static_cast<KeyedService*>(
      new NotificationScheduleServiceImpl(std::move(init_aware_scheduler)));
}

}  // namespace notifications
