// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/browser/notifications/notification_trigger_constants.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/mock_platform_notification_service.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"
#include "url/gurl.h"

using base::Time;
using base::TimeDelta;

namespace content {

// Fake Service Worker registration id to use in tests requiring one.
const int64_t kFakeServiceWorkerRegistrationId = 42;

class NotificationBrowserClient : public TestContentBrowserClient {
 public:
  NotificationBrowserClient()
      : platform_notification_service_(
            std::make_unique<MockPlatformNotificationService>()) {}

  PlatformNotificationService* GetPlatformNotificationService() override {
    return platform_notification_service_.get();
  }

 private:
  std::unique_ptr<PlatformNotificationService> platform_notification_service_;
};

class PlatformNotificationContextTriggerTest : public ::testing::Test {
 public:
  PlatformNotificationContextTriggerTest()
      : thread_bundle_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
            base::test::ScopedTaskEnvironment::NowSource::
                MAIN_THREAD_MOCK_TIME),
        success_(false) {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kNotificationTriggers);
    SetBrowserClientForTesting(&notification_browser_client_);
    platform_notification_service_ =
        notification_browser_client_.GetPlatformNotificationService();
    platform_notification_context_ =
        base::MakeRefCounted<PlatformNotificationContextImpl>(
            base::FilePath(), &browser_context_, nullptr);
    platform_notification_context_->SetTaskRunnerForTesting(
        base::ThreadTaskRunnerHandle::Get());
    platform_notification_context_->Initialize();
    base::RunLoop().RunUntilIdle();
  }

  // Callback to provide when writing a notification to the database.
  void DidWriteNotificationData(bool success,
                                const std::string& notification_id) {
    success_ = success;
  }

  // Callback to provide when deleting notification data from the database.
  void DidDeleteNotificationData(bool success) { success_ = success; }

  // Callback to provide when getting displayed notifications from the
  // PlatformNotificationService.
  void DidGetDisplayedNotifications(std::set<std::string> notification_ids,
                                    bool supports_synchronization) {
    displayed_notification_ids_ = notification_ids;
  }

 protected:
  void WriteNotificationData(const std::string& tag,
                             base::Optional<base::Time> timestamp) {
    ASSERT_TRUE(
        TryWriteNotificationData("https://example.com", tag, timestamp));
  }

  bool TryWriteNotificationData(const std::string& url,
                                const std::string& tag,
                                base::Optional<base::Time> timestamp) {
    GURL origin(url);
    NotificationDatabaseData notification_database_data;
    notification_database_data.origin = origin;
    notification_database_data.service_worker_registration_id =
        kFakeServiceWorkerRegistrationId;
    notification_database_data.notification_data.show_trigger_timestamp =
        timestamp;
    notification_database_data.notification_data.tag = tag;

    platform_notification_context_->WriteNotificationData(
        next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
        origin, notification_database_data,
        base::BindOnce(
            &PlatformNotificationContextTriggerTest::DidWriteNotificationData,
            base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    return success_;
  }

  std::set<std::string> GetDisplayedNotifications() {
    platform_notification_service_->GetDisplayedNotifications(
        &browser_context_,
        base::BindOnce(&PlatformNotificationContextTriggerTest::
                           DidGetDisplayedNotifications,
                       base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    return displayed_notification_ids_;
  }

  TestBrowserThreadBundle thread_bundle_;  // Must be first member
  TestBrowserContext browser_context_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  NotificationBrowserClient notification_browser_client_;
  PlatformNotificationService* platform_notification_service_;
  scoped_refptr<PlatformNotificationContextImpl> platform_notification_context_;

  // Returns the next persistent notification id for tests.
  int64_t next_persistent_notification_id() {
    return next_persistent_notification_id_++;
  }

  bool success_;
  NotificationDatabaseData database_data_;
  std::string notification_id_;
  int64_t next_persistent_notification_id_ = 1;
  std::set<std::string> displayed_notification_ids_;
};

TEST_F(PlatformNotificationContextTriggerTest, TriggerInFuture) {
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));
  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  // Wait until the trigger timestamp is reached.
  thread_bundle_.FastForwardBy(TimeDelta::FromSeconds(10));
  ASSERT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(PlatformNotificationContextTriggerTest, TriggerInPast) {
  // Trigger timestamp in the past should immediately trigger.
  WriteNotificationData("1", Time::Now() - TimeDelta::FromSeconds(10));
  ASSERT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(PlatformNotificationContextTriggerTest,
       OverwriteExistingTriggerInFuture) {
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));
  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  thread_bundle_.FastForwardBy(TimeDelta::FromSeconds(5));
  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  // Overwrites the scheduled notifications with a new trigger timestamp.
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));

  thread_bundle_.FastForwardBy(TimeDelta::FromSeconds(5));
  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  thread_bundle_.FastForwardBy(TimeDelta::FromSeconds(5));
  ASSERT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(PlatformNotificationContextTriggerTest, OverwriteExistingTriggerToPast) {
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));
  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  thread_bundle_.FastForwardBy(TimeDelta::FromSeconds(5));

  // Overwrites the scheduled notifications with a new trigger timestamp.
  WriteNotificationData("1", Time::Now() - TimeDelta::FromSeconds(10));
  ASSERT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(PlatformNotificationContextTriggerTest,
       OverwriteDisplayedNotificationToPast) {
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));
  thread_bundle_.FastForwardBy(TimeDelta::FromSeconds(10));

  // Overwrites a displayed notification with a trigger timestamp in the past.
  WriteNotificationData("1", Time::Now() - TimeDelta::FromSeconds(10));
  ASSERT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(PlatformNotificationContextTriggerTest,
       OverwriteDisplayedNotificationToFuture) {
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));
  thread_bundle_.FastForwardBy(TimeDelta::FromSeconds(10));

  // Overwrites a displayed notification which hides it until the trigger
  // timestamp is reached.
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));

  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  thread_bundle_.FastForwardBy(TimeDelta::FromSeconds(10));
  ASSERT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(PlatformNotificationContextTriggerTest,
       LimitsNumberOfScheduledNotificationsPerOrigin) {
  for (int i = 1; i <= kMaximumScheduledNotificationsPerOrigin; ++i) {
    WriteNotificationData(std::to_string(i),
                          Time::Now() + TimeDelta::FromSeconds(i));
  }

  ASSERT_FALSE(TryWriteNotificationData(
      "https://example.com",
      std::to_string(kMaximumScheduledNotificationsPerOrigin + 1),
      Time::Now() +
          TimeDelta::FromSeconds(kMaximumScheduledNotificationsPerOrigin + 1)));

  ASSERT_TRUE(TryWriteNotificationData(
      "https://example2.com",
      std::to_string(kMaximumScheduledNotificationsPerOrigin + 1),
      Time::Now() +
          TimeDelta::FromSeconds(kMaximumScheduledNotificationsPerOrigin + 1)));
}

TEST_F(PlatformNotificationContextTriggerTest, EnforcesLimitOnUpdate) {
  for (int i = 1; i <= kMaximumScheduledNotificationsPerOrigin; ++i) {
    WriteNotificationData(std::to_string(i),
                          Time::Now() + TimeDelta::FromSeconds(i));
  }

  ASSERT_TRUE(TryWriteNotificationData(
      "https://example.com",
      std::to_string(kMaximumScheduledNotificationsPerOrigin + 1),
      base::nullopt));

  ASSERT_FALSE(TryWriteNotificationData(
      "https://example.com",
      std::to_string(kMaximumScheduledNotificationsPerOrigin + 1),
      Time::Now() +
          TimeDelta::FromSeconds(kMaximumScheduledNotificationsPerOrigin + 1)));
}

}  // namespace content
