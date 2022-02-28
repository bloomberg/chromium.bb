// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "chromeos/assistant/internal/action/cros_action_module.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/services/libassistant/public/cpp/assistant_notification.h"
#include "chromeos/services/libassistant/public/mojom/notification_delegate.mojom-forward.h"
#include "chromeos/services/libassistant/test_support/libassistant_service_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
namespace libassistant {

namespace {

class NotificationDelegateMock : public mojom::NotificationDelegate {
 public:
  NotificationDelegateMock() = default;
  NotificationDelegateMock(const NotificationDelegateMock&) = delete;
  NotificationDelegateMock& operator=(const NotificationDelegateMock&) = delete;
  ~NotificationDelegateMock() override = default;

  // mojom::NotificationDelegate implementation:
  MOCK_METHOD(void,
              AddOrUpdateNotification,
              (assistant::AssistantNotification notification));
  MOCK_METHOD(void, RemoveAllNotifications, (bool from_server));
  MOCK_METHOD(void,
              RemoveNotificationByGroupingKey,
              (const std::string& grouping_key, bool from_server));

  void Bind(
      mojo::PendingReceiver<chromeos::libassistant::mojom::NotificationDelegate>
          pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<mojom::NotificationDelegate> receiver_{this};
};

// Helper class to fire interaction response handlers for tests.
// TODO(meilinw): move it to a separate class since it is used both here and
// by |ConversationObserverTest|.
class CrosActionModuleHelper {
 public:
  explicit CrosActionModuleHelper(
      assistant::action::CrosActionModule* action_module)
      : action_module_(*action_module) {}
  CrosActionModuleHelper(const CrosActionModuleHelper&) = delete;
  CrosActionModuleHelper& operator=(const CrosActionModuleHelper&) = delete;
  ~CrosActionModuleHelper() = default;

  void ShowNotification(const assistant::action::Notification& notification) {
    for (auto* observer : action_observers())
      observer->OnShowNotification(notification);
  }

 private:
  const std::vector<assistant::action::AssistantActionObserver*>&
  action_observers() {
    return action_module_.GetActionObserversForTesting();
  }

  const assistant::action::CrosActionModule& action_module_;
};

}  // namespace

class NotificationDelegateTest : public ::testing::Test {
 public:
  NotificationDelegateTest() = default;
  NotificationDelegateTest(const NotificationDelegateTest&) = delete;
  NotificationDelegateTest& operator=(const NotificationDelegateTest&) = delete;
  ~NotificationDelegateTest() override = default;

  void SetUp() override {
    delegate_mock_.Bind(
        service_tester_.GetNotificationDelegatePendingReceiver());
    service_tester_.Start();
    action_module_helper_ = std::make_unique<CrosActionModuleHelper>(
        static_cast<assistant::action::CrosActionModule*>(
            service_tester_.assistant_manager_internal().action_module()));
  }

  assistant_client::AssistantManagerDelegate& assistant_manager_delegate() {
    return *service_tester_.assistant_manager_internal()
                .assistant_manager_delegate();
  }

  NotificationDelegateMock& delegate_mock() { return delegate_mock_; }

  CrosActionModuleHelper& action_module_helper() {
    return *action_module_helper_.get();
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_;
  ::testing::StrictMock<NotificationDelegateMock> delegate_mock_;
  LibassistantServiceTester service_tester_;
  std::unique_ptr<CrosActionModuleHelper> action_module_helper_;
};

TEST_F(NotificationDelegateTest, ShouldInvokeAddOrUpdateNotification) {
  assistant::action::Notification input_notification{
      /*title=*/"title",
      /*text=*/"text",
      /*action_url=*/"https://action-url/",
      /*notification_id=*/"notification_id",
      /*consistency_token=*/"consistency_token",
      /*opaque_token=*/"opaque_token",
      /*grouping_key=*/"grouping_key",
      /*obfuscated_gaia_id=*/"obfuscated_gaia_id",
      /*expiry_timestamp_ms=*/100,
  };

  EXPECT_CALL(delegate_mock(), AddOrUpdateNotification)
      .WillOnce(testing::Invoke(
          [&](const assistant::AssistantNotification& output_notification) {
            EXPECT_EQ("title", output_notification.title);
            EXPECT_EQ("text", output_notification.message);
            EXPECT_EQ("notification_id", output_notification.server_id);
            EXPECT_EQ("notification_id", output_notification.client_id);
            EXPECT_EQ("consistency_token",
                      output_notification.consistency_token);
            EXPECT_EQ("opaque_token", output_notification.opaque_token);
            EXPECT_EQ("grouping_key", output_notification.grouping_key);
            EXPECT_EQ("obfuscated_gaia_id",
                      output_notification.obfuscated_gaia_id);
            EXPECT_EQ(base::Time::FromJavaTime(100),
                      output_notification.expiry_time);
            EXPECT_EQ(true, output_notification.from_server);
          }));

  action_module_helper().ShowNotification(input_notification);
  delegate_mock().FlushForTesting();
}

TEST_F(NotificationDelegateTest, ShouldInvokeRemoveAllNotifications) {
  EXPECT_CALL(delegate_mock(), RemoveAllNotifications(/*from_server=*/true));

  // Pass in empty |grouping_key| should trigger all notifications being
  // removed.
  assistant_manager_delegate().OnNotificationRemoved(/*grouping_key=*/"");
  delegate_mock().FlushForTesting();
}

TEST_F(NotificationDelegateTest, ShouldInvokeRemoveNotificationByGroupingKey) {
  const std::string grouping_id = "grouping-id";
  EXPECT_CALL(delegate_mock(), RemoveNotificationByGroupingKey(
                                   /*id=*/grouping_id, /*from_server=*/true));

  // Pass in non-empty |grouping_key| will trigger specific group of
  // notifications being removed.
  assistant_manager_delegate().OnNotificationRemoved(grouping_id);
  delegate_mock().FlushForTesting();
}

}  // namespace libassistant
}  // namespace chromeos
