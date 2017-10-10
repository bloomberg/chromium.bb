// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/message_center/public/cpp/message_center_constants.h"

using base::UTF8ToUTF16;

namespace message_center {

class MessageCenterImplTest : public testing::Test,
                              public MessageCenterObserver {
 public:
  MessageCenterImplTest() {}

  void SetUp() override {
    MessageCenter::Initialize();
    message_center_ = MessageCenter::Get();
    loop_.reset(new base::MessageLoop);
    run_loop_.reset(new base::RunLoop());
    closure_ = run_loop_->QuitClosure();
  }

  void TearDown() override {
    run_loop_.reset();
    loop_.reset();
    message_center_ = NULL;
    MessageCenter::Shutdown();
  }

  MessageCenter* message_center() const { return message_center_; }
  NotifierSettingsObserver* notifier_settings_observer() const {
    return static_cast<NotifierSettingsObserver*>(message_center_impl());
  }
  MessageCenterImpl* message_center_impl() const {
    return reinterpret_cast<MessageCenterImpl*>(message_center_);
  }

  base::RunLoop* run_loop() const { return run_loop_.get(); }
  base::Closure closure() const { return closure_; }

 protected:
  Notification* CreateSimpleNotification(const std::string& id) {
    return CreateNotificationWithNotifierId(
        id,
        "app1",
        NOTIFICATION_TYPE_SIMPLE);
  }

  Notification* CreateSimpleNotificationWithNotifierId(
    const std::string& id, const std::string& notifier_id) {
    return CreateNotificationWithNotifierId(
        id,
        notifier_id,
        NOTIFICATION_TYPE_SIMPLE);
  }

  Notification* CreateNotification(const std::string& id,
                                   message_center::NotificationType type) {
    return CreateNotificationWithNotifierId(id, "app1", type);
  }

  Notification* CreateNotificationWithNotifierId(
      const std::string& id,
      const std::string& notifier_id,
      message_center::NotificationType type) {
    RichNotificationData optional_fields;
    optional_fields.buttons.push_back(ButtonInfo(UTF8ToUTF16("foo")));
    optional_fields.buttons.push_back(ButtonInfo(UTF8ToUTF16("foo")));
    return new Notification(type, id, UTF8ToUTF16("title"), UTF8ToUTF16(id),
                            gfx::Image() /* icon */,
                            base::string16() /* display_source */, GURL(),
                            NotifierId(NotifierId::APPLICATION, notifier_id),
                            optional_fields, NULL);
  }

 private:
  MessageCenter* message_center_;
  std::unique_ptr<base::MessageLoop> loop_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::Closure closure_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterImplTest);
};

namespace {

class ToggledNotificationBlocker : public NotificationBlocker {
 public:
  explicit ToggledNotificationBlocker(MessageCenter* message_center)
      : NotificationBlocker(message_center),
        notifications_enabled_(true) {}
  ~ToggledNotificationBlocker() override {}

  void SetNotificationsEnabled(bool enabled) {
    if (notifications_enabled_ != enabled) {
      notifications_enabled_ = enabled;
      NotifyBlockingStateChanged();
    }
  }

  // NotificationBlocker overrides:
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override {
    return notifications_enabled_;
  }

 private:
  bool notifications_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ToggledNotificationBlocker);
};

class PopupNotificationBlocker : public ToggledNotificationBlocker {
 public:
  PopupNotificationBlocker(MessageCenter* message_center,
                           const NotifierId& allowed_notifier)
      : ToggledNotificationBlocker(message_center),
        allowed_notifier_(allowed_notifier) {}
  ~PopupNotificationBlocker() override {}

  // NotificationBlocker overrides:
  bool ShouldShowNotificationAsPopup(
      const Notification& notification) const override {
    return (notification.notifier_id() == allowed_notifier_) ||
        ToggledNotificationBlocker::ShouldShowNotificationAsPopup(
            notification);
  }

 private:
  NotifierId allowed_notifier_;

  DISALLOW_COPY_AND_ASSIGN(PopupNotificationBlocker);
};

class TotalNotificationBlocker : public PopupNotificationBlocker {
 public:
  TotalNotificationBlocker(MessageCenter* message_center,
                           const NotifierId& allowed_notifier)
      : PopupNotificationBlocker(message_center, allowed_notifier) {}
  ~TotalNotificationBlocker() override {}

  // NotificationBlocker overrides:
  bool ShouldShowNotification(const Notification& notification) const override {
    return ShouldShowNotificationAsPopup(notification);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TotalNotificationBlocker);
};

bool PopupNotificationsContain(
    const NotificationList::PopupNotifications& popups,
    const std::string& id) {
  for (NotificationList::PopupNotifications::const_iterator iter =
           popups.begin(); iter != popups.end(); ++iter) {
    if ((*iter)->id() == id)
      return true;
  }
  return false;
}

// Right now, MessageCenter::HasNotification() returns regardless of blockers.
bool NotificationsContain(
    const NotificationList::Notifications& notifications,
    const std::string& id) {
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    if ((*iter)->id() == id)
      return true;
  }
  return false;
}

}  // namespace

namespace internal {

class MockPopupTimersController : public PopupTimersController {
 public:
  MockPopupTimersController(MessageCenter* message_center,
                            base::Closure quit_closure)
      : PopupTimersController(message_center),
        timer_finished_(0),
        quit_closure_(quit_closure) {}
  ~MockPopupTimersController() override {}

  void TimerFinished(const std::string& id) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
    timer_finished_++;
    last_id_ = id;
  }

  int timer_finished() const { return timer_finished_; }
  const std::string& last_id() const { return last_id_; }

 private:
  int timer_finished_;
  std::string last_id_;
  base::Closure quit_closure_;
};

TEST_F(MessageCenterImplTest, PopupTimersEmptyController) {
  std::unique_ptr<PopupTimersController> popup_timers_controller =
      std::make_unique<PopupTimersController>(message_center());

  // Test that all functions succed without any timers created.
  popup_timers_controller->PauseAll();
  popup_timers_controller->StartAll();
  popup_timers_controller->CancelAll();
  popup_timers_controller->TimerFinished("unknown");
  popup_timers_controller->CancelTimer("unknown");
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartTimer) {
  std::unique_ptr<MockPopupTimersController> popup_timers_controller =
      std::make_unique<MockPopupTimersController>(message_center(), closure());
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  run_loop()->Run();
  EXPECT_EQ(popup_timers_controller->timer_finished(), 1);
}

TEST_F(MessageCenterImplTest, PopupTimersControllerCancelTimer) {
  std::unique_ptr<MockPopupTimersController> popup_timers_controller =
      std::make_unique<MockPopupTimersController>(message_center(), closure());
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->CancelTimer("test");
  run_loop()->RunUntilIdle();

  EXPECT_EQ(popup_timers_controller->timer_finished(), 0);
}

TEST_F(MessageCenterImplTest, PopupTimersControllerPauseAllTimers) {
  std::unique_ptr<MockPopupTimersController> popup_timers_controller =
      std::make_unique<MockPopupTimersController>(message_center(), closure());
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->PauseAll();
  run_loop()->RunUntilIdle();

  EXPECT_EQ(popup_timers_controller->timer_finished(), 0);
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartAllTimers) {
  std::unique_ptr<MockPopupTimersController> popup_timers_controller =
      std::make_unique<MockPopupTimersController>(message_center(), closure());
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->PauseAll();
  popup_timers_controller->StartAll();
  run_loop()->Run();

  EXPECT_EQ(popup_timers_controller->timer_finished(), 1);
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartMultipleTimers) {
  std::unique_ptr<MockPopupTimersController> popup_timers_controller =
      std::make_unique<MockPopupTimersController>(message_center(), closure());
  popup_timers_controller->StartTimer("test", base::TimeDelta::Max());
  popup_timers_controller->StartTimer("test2",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->StartTimer("test3", base::TimeDelta::Max());
  popup_timers_controller->PauseAll();
  popup_timers_controller->StartAll();
  run_loop()->Run();

  EXPECT_EQ(popup_timers_controller->last_id(), "test2");
  EXPECT_EQ(popup_timers_controller->timer_finished(), 1);
}

TEST_F(MessageCenterImplTest, PopupTimersControllerRestartOnUpdate) {
  scoped_refptr<base::SingleThreadTaskRunner> old_task_runner =
      base::ThreadTaskRunnerHandle::Get();

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner(
      new base::TestMockTimeTaskRunner(base::Time::Now(),
                                       base::TimeTicks::Now()));
  base::MessageLoop::current()->SetTaskRunner(task_runner);

  NotifierId notifier_id(GURL("https://example.com"));

  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id1", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id, RichNotificationData(), NULL)));

  std::unique_ptr<MockPopupTimersController> popup_timers_controller =
      std::make_unique<MockPopupTimersController>(message_center(), closure());

  popup_timers_controller->OnNotificationDisplayed("id1", DISPLAY_SOURCE_POPUP);
  ASSERT_EQ(popup_timers_controller->timer_finished(), 0);

  // Fast forward the |task_runner| by one second less than the auto-close timer
  // frequency for Web Notifications. (As set by the |notifier_id|.)
  task_runner->FastForwardBy(
      base::TimeDelta::FromSeconds(kAutocloseHighPriorityDelaySeconds - 1));
  ASSERT_EQ(popup_timers_controller->timer_finished(), 0);

  // Trigger a replacement of the notification in the timer controller.
  popup_timers_controller->OnNotificationUpdated("id1");

  // Fast forward the |task_runner| by one second less than the auto-close timer
  // frequency for Web Notifications again. It should have been reset.
  task_runner->FastForwardBy(
      base::TimeDelta::FromSeconds(kAutocloseHighPriorityDelaySeconds - 1));
  ASSERT_EQ(popup_timers_controller->timer_finished(), 0);

  // Now fast forward the |task_runner| by two seconds (to avoid flakiness),
  // after which the timer should have fired.
  task_runner->FastForwardBy(base::TimeDelta::FromSeconds(2));
  ASSERT_EQ(popup_timers_controller->timer_finished(), 1);

  base::MessageLoop::current()->SetTaskRunner(old_task_runner);
}

TEST_F(MessageCenterImplTest, NotificationBlocker) {
  NotifierId notifier_id(NotifierId::APPLICATION, "app1");
  // Multiple blockers to verify the case that one blocker blocks but another
  // doesn't.
  ToggledNotificationBlocker blocker1(message_center());
  ToggledNotificationBlocker blocker2(message_center());

  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id1", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id, RichNotificationData(), NULL)));
  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id2", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id, RichNotificationData(), NULL)));
  EXPECT_EQ(2u, message_center()->GetPopupNotifications().size());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // Block all notifications. All popups are gone and message center should be
  // hidden.
  blocker1.SetNotificationsEnabled(false);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // Updates |blocker2| state, which doesn't affect the global state.
  blocker2.SetNotificationsEnabled(false);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  blocker2.SetNotificationsEnabled(true);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // If |blocker2| blocks, then unblocking blocker1 doesn't change the global
  // state.
  blocker2.SetNotificationsEnabled(false);
  blocker1.SetNotificationsEnabled(true);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // Unblock both blockers, which recovers the global state, but the popups
  // aren't shown.
  blocker2.SetNotificationsEnabled(true);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());
}

TEST_F(MessageCenterImplTest, NotificationsDuringBlocked) {
  NotifierId notifier_id(NotifierId::APPLICATION, "app1");
  ToggledNotificationBlocker blocker(message_center());

  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id1", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id, RichNotificationData(), NULL)));
  EXPECT_EQ(1u, message_center()->GetPopupNotifications().size());
  EXPECT_EQ(1u, message_center()->GetVisibleNotifications().size());

  // Create a notification during blocked. Still no popups.
  blocker.SetNotificationsEnabled(false);
  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id2", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id, RichNotificationData(), NULL)));
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // Unblock notifications, the id1 should appear as a popup.
  blocker.SetNotificationsEnabled(true);
  NotificationList::PopupNotifications popups =
      message_center()->GetPopupNotifications();
  EXPECT_EQ(1u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());
}

// Similar to other blocker cases but this test case allows |notifier_id2| even
// in blocked.
TEST_F(MessageCenterImplTest, NotificationBlockerAllowsPopups) {
  NotifierId notifier_id1(NotifierId::APPLICATION, "app1");
  NotifierId notifier_id2(NotifierId::APPLICATION, "app2");
  PopupNotificationBlocker blocker(message_center(), notifier_id2);

  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id1", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id1, RichNotificationData(), NULL)));
  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id2", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id2, RichNotificationData(), NULL)));

  // "id1" is closed but "id2" is still visible as a popup.
  blocker.SetNotificationsEnabled(false);
  NotificationList::PopupNotifications popups =
      message_center()->GetPopupNotifications();
  EXPECT_EQ(1u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id3", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id1, RichNotificationData(), NULL)));
  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id4", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id2, RichNotificationData(), NULL)));
  popups = message_center()->GetPopupNotifications();
  EXPECT_EQ(2u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_TRUE(PopupNotificationsContain(popups, "id4"));
  EXPECT_EQ(4u, message_center()->GetVisibleNotifications().size());

  blocker.SetNotificationsEnabled(true);
  popups = message_center()->GetPopupNotifications();
  EXPECT_EQ(3u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_TRUE(PopupNotificationsContain(popups, "id3"));
  EXPECT_TRUE(PopupNotificationsContain(popups, "id4"));
  EXPECT_EQ(4u, message_center()->GetVisibleNotifications().size());
}

// TotalNotificationBlocker suppresses showing notifications even from the list.
// This would provide the feature to 'separated' message centers per-profile for
// ChromeOS multi-login.
TEST_F(MessageCenterImplTest, TotalNotificationBlocker) {
  NotifierId notifier_id1(NotifierId::APPLICATION, "app1");
  NotifierId notifier_id2(NotifierId::APPLICATION, "app2");
  TotalNotificationBlocker blocker(message_center(), notifier_id2);

  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id1", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id1, RichNotificationData(), NULL)));
  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id2", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id2, RichNotificationData(), NULL)));

  // "id1" becomes invisible while "id2" is still visible.
  blocker.SetNotificationsEnabled(false);
  EXPECT_EQ(1u, message_center()->NotificationCount());
  NotificationList::Notifications notifications =
      message_center()->GetVisibleNotifications();
  EXPECT_FALSE(NotificationsContain(notifications, "id1"));
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));

  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id3", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id1, RichNotificationData(), NULL)));
  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id4", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id2, RichNotificationData(), NULL)));
  EXPECT_EQ(2u, message_center()->NotificationCount());
  notifications = message_center()->GetVisibleNotifications();
  EXPECT_FALSE(NotificationsContain(notifications, "id1"));
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));
  EXPECT_FALSE(NotificationsContain(notifications, "id3"));
  EXPECT_TRUE(NotificationsContain(notifications, "id4"));

  blocker.SetNotificationsEnabled(true);
  EXPECT_EQ(4u, message_center()->NotificationCount());
  notifications = message_center()->GetVisibleNotifications();
  EXPECT_TRUE(NotificationsContain(notifications, "id1"));
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));
  EXPECT_TRUE(NotificationsContain(notifications, "id3"));
  EXPECT_TRUE(NotificationsContain(notifications, "id4"));

  // Remove just visible notifications.
  blocker.SetNotificationsEnabled(false);
  message_center()->RemoveAllNotifications(
      false /* by_user */, MessageCenter::RemoveType::NON_PINNED);
  EXPECT_EQ(0u, message_center()->NotificationCount());
  blocker.SetNotificationsEnabled(true);
  EXPECT_EQ(2u, message_center()->NotificationCount());
  notifications = message_center()->GetVisibleNotifications();
  EXPECT_TRUE(NotificationsContain(notifications, "id1"));
  EXPECT_FALSE(NotificationsContain(notifications, "id2"));
  EXPECT_TRUE(NotificationsContain(notifications, "id3"));
  EXPECT_FALSE(NotificationsContain(notifications, "id4"));

  // And remove all including invisible notifications.
  blocker.SetNotificationsEnabled(false);
  message_center()->RemoveAllNotifications(false /* by_user */,
                                           MessageCenter::RemoveType::ALL);
  EXPECT_EQ(0u, message_center()->NotificationCount());
}

TEST_F(MessageCenterImplTest, RemoveAllNotifications) {
  NotifierId notifier_id1(NotifierId::APPLICATION, "app1");
  NotifierId notifier_id2(NotifierId::APPLICATION, "app2");

  TotalNotificationBlocker blocker(message_center(), notifier_id1);
  blocker.SetNotificationsEnabled(false);

  // Notification 1: Visible, non-pinned
  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id1", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id1, RichNotificationData(), NULL)));

  // Notification 2: Invisible, non-pinned
  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id2", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id2, RichNotificationData(), NULL)));

  // Remove all the notifications which are visible and non-pinned.
  message_center()->RemoveAllNotifications(
      false /* by_user */, MessageCenter::RemoveType::NON_PINNED);

  EXPECT_EQ(0u, message_center()->NotificationCount());
  blocker.SetNotificationsEnabled(true);  // Show invisible notifications.
  EXPECT_EQ(1u, message_center()->NotificationCount());

  NotificationList::Notifications notifications =
      message_center()->GetVisibleNotifications();
  // Notification 1 should be removed.
  EXPECT_FALSE(NotificationsContain(notifications, "id1"));
  // Notification 2 shouldn't be removed since it was invisible.
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));
}

#if defined(OS_CHROMEOS)
TEST_F(MessageCenterImplTest, RemoveAllNotificationsWithPinned) {
  NotifierId notifier_id1(NotifierId::APPLICATION, "app1");
  NotifierId notifier_id2(NotifierId::APPLICATION, "app2");

  TotalNotificationBlocker blocker(message_center(), notifier_id1);
  blocker.SetNotificationsEnabled(false);

  // Notification 1: Visible, non-pinned
  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id1", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id1, RichNotificationData(), NULL)));

  // Notification 2: Invisible, non-pinned
  message_center()->AddNotification(std::unique_ptr<Notification>(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id2", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id2, RichNotificationData(), NULL)));

  // Notification 3: Visible, pinned
  std::unique_ptr<Notification> notification3(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id3", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id1, RichNotificationData(), NULL));
  notification3->set_pinned(true);
  message_center()->AddNotification(std::move(notification3));

  // Notification 4: Invisible, pinned
  std::unique_ptr<Notification> notification4(
      new Notification(NOTIFICATION_TYPE_SIMPLE, "id4", UTF8ToUTF16("title"),
                       UTF8ToUTF16("message"), gfx::Image() /* icon */,
                       base::string16() /* display_source */, GURL(),
                       notifier_id2, RichNotificationData(), NULL));
  notification4->set_pinned(true);
  message_center()->AddNotification(std::move(notification4));

  // Remove all the notifications which are visible and non-pinned.
  message_center()->RemoveAllNotifications(
      false /* by_user */, MessageCenter::RemoveType::NON_PINNED);

  EXPECT_EQ(1u, message_center()->NotificationCount());
  blocker.SetNotificationsEnabled(true);  // Show invisible notifications.
  EXPECT_EQ(3u, message_center()->NotificationCount());

  NotificationList::Notifications notifications =
      message_center()->GetVisibleNotifications();
  // Notification 1 should be removed.
  EXPECT_FALSE(NotificationsContain(notifications, "id1"));
  // Notification 2 shouldn't be removed since it was invisible.
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));
  // Notification 3 shouldn't be removed since it was pinned.
  EXPECT_TRUE(NotificationsContain(notifications, "id3"));
  // Notification 4 shouldn't be removed since it was invisible and pinned.
  EXPECT_TRUE(NotificationsContain(notifications, "id4"));
}
#endif

#if defined(OS_CHROMEOS)
TEST_F(MessageCenterImplTest, CachedUnreadCount) {
  message_center()->AddNotification(
      std::unique_ptr<Notification>(CreateSimpleNotification("id1")));
  message_center()->AddNotification(
      std::unique_ptr<Notification>(CreateSimpleNotification("id2")));
  message_center()->AddNotification(
      std::unique_ptr<Notification>(CreateSimpleNotification("id3")));
  ASSERT_EQ(3u, message_center()->UnreadNotificationCount());

  // Mark 'displayed' on all notifications by using for-loop. This shouldn't
  // recreate |notifications| inside of the loop.
  const NotificationList::Notifications& notifications =
      message_center()->GetVisibleNotifications();
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    message_center()->DisplayedNotification(
        (*iter)->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
  }
  EXPECT_EQ(0u, message_center()->UnreadNotificationCount());

  // Imitate the timeout, which recovers the unread count. Again, this shouldn't
  // recreate |notifications| inside of the loop.
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    message_center()->MarkSinglePopupAsShown((*iter)->id(), false);
  }
  EXPECT_EQ(3u, message_center()->UnreadNotificationCount());

  // Opening the message center will reset the unread count.
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);
  EXPECT_EQ(0u, message_center()->UnreadNotificationCount());
}
#endif  // OS_CHROMEOS

TEST_F(MessageCenterImplTest, NotifierEnabledChanged) {
  ASSERT_EQ(0u, message_center()->NotificationCount());
  message_center()->AddNotification(std::unique_ptr<Notification>(
      CreateSimpleNotificationWithNotifierId("id1-1", "app1")));
  message_center()->AddNotification(std::unique_ptr<Notification>(
      CreateSimpleNotificationWithNotifierId("id1-2", "app1")));
  message_center()->AddNotification(std::unique_ptr<Notification>(
      CreateSimpleNotificationWithNotifierId("id1-3", "app1")));
  message_center()->AddNotification(std::unique_ptr<Notification>(
      CreateSimpleNotificationWithNotifierId("id2-1", "app2")));
  message_center()->AddNotification(std::unique_ptr<Notification>(
      CreateSimpleNotificationWithNotifierId("id2-2", "app2")));
  message_center()->AddNotification(std::unique_ptr<Notification>(
      CreateSimpleNotificationWithNotifierId("id2-3", "app2")));
  message_center()->AddNotification(std::unique_ptr<Notification>(
      CreateSimpleNotificationWithNotifierId("id2-4", "app2")));
  message_center()->AddNotification(std::unique_ptr<Notification>(
      CreateSimpleNotificationWithNotifierId("id2-5", "app2")));
  ASSERT_EQ(8u, message_center()->NotificationCount());

  // Enabling an extension should have no effect on the count.
  notifier_settings_observer()->NotifierEnabledChanged(
      NotifierId(NotifierId::APPLICATION, "app1"), true);
  ASSERT_EQ(8u, message_center()->NotificationCount());

  // Removing all of app2's notifications should only leave app1's.
  notifier_settings_observer()->NotifierEnabledChanged(
      NotifierId(NotifierId::APPLICATION, "app2"), false);
  ASSERT_EQ(3u, message_center()->NotificationCount());

  // Removal operations should be idempotent.
  notifier_settings_observer()->NotifierEnabledChanged(
      NotifierId(NotifierId::APPLICATION, "app2"), false);
  ASSERT_EQ(3u, message_center()->NotificationCount());

  // Now we remove the remaining notifications.
  notifier_settings_observer()->NotifierEnabledChanged(
      NotifierId(NotifierId::APPLICATION, "app1"), false);
  ASSERT_EQ(0u, message_center()->NotificationCount());
}

TEST_F(MessageCenterImplTest, UpdateWhileMessageCenterVisible) {
  std::string id("id1");
  std::string id2("id2");
  NotifierId notifier_id1(NotifierId::APPLICATION, "app1");

  // First, add and update a notification to ensure updates happen
  // normally.
  std::unique_ptr<Notification> notification(CreateSimpleNotification(id));
  message_center()->AddNotification(std::move(notification));
  notification.reset(CreateSimpleNotification(id2));
  message_center()->UpdateNotification(id, std::move(notification));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id2));
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id));

  // Then open the message center.
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);

  // Then update a notification; the update should have propagated.
  notification.reset(CreateSimpleNotification(id));
  message_center()->UpdateNotification(id2, std::move(notification));
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id2));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id));
}

TEST_F(MessageCenterImplTest, AddWhileMessageCenterVisible) {
  std::string id("id1");

  // Then open the message center.
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);

  // Add a notification and confirm the adding should have propagated.
  std::unique_ptr<Notification> notification(CreateSimpleNotification(id));
  message_center()->AddNotification(std::move(notification));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id));
}

TEST_F(MessageCenterImplTest, RemoveWhileMessageCenterVisible) {
  std::string id("id1");

  // First, add a notification to ensure updates happen normally.
  std::unique_ptr<Notification> notification(CreateSimpleNotification(id));
  message_center()->AddNotification(std::move(notification));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id));

  // Then open the message center.
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);

  // Then update a notification; the update should have propagated.
  message_center()->RemoveNotification(id, false);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id));
}

}  // namespace internal
}  // namespace message_center
