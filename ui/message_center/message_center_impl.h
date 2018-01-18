// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_IMPL_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/message_center/change_queue.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/notifier_id.h"
#include "ui/message_center/popup_timers_controller.h"

namespace message_center {

namespace internal {
class ScopedNotificationsIterationLock;
};

// The default implementation of MessageCenter.
class MESSAGE_CENTER_EXPORT MessageCenterImpl
    : public MessageCenter,
      public NotificationBlocker::Observer {
 public:
  MessageCenterImpl();
  ~MessageCenterImpl() override;

  // MessageCenter overrides:
  void AddObserver(MessageCenterObserver* observer) override;
  void RemoveObserver(MessageCenterObserver* observer) override;
  void AddNotificationBlocker(NotificationBlocker* blocker) override;
  void RemoveNotificationBlocker(NotificationBlocker* blocker) override;
  void SetVisibility(Visibility visible) override;
  bool IsMessageCenterVisible() const override;
  size_t NotificationCount() const override;
  bool HasPopupNotifications() const override;
  bool IsQuietMode() const override;
  message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override;
  const NotificationList::Notifications& GetVisibleNotifications() override;
  NotificationList::PopupNotifications GetPopupNotifications() override;
  void AddNotification(std::unique_ptr<Notification> notification) override;
  void UpdateNotification(
      const std::string& old_id,
      std::unique_ptr<Notification> new_notification) override;
  void RemoveNotification(const std::string& id, bool by_user) override;
  void RemoveNotificationsForNotifierId(const NotifierId& notifier_id) override;
  void RemoveAllNotifications(bool by_user, RemoveType type) override;
  void SetNotificationIcon(const std::string& notification_id,
                           const gfx::Image& image) override;
  void SetNotificationImage(const std::string& notification_id,
                            const gfx::Image& image) override;
  void SetNotificationButtonIcon(const std::string& notification_id,
                                 int button_index,
                                 const gfx::Image& image) override;
  void ClickOnNotification(const std::string& id) override;
  void ClickOnNotificationButton(const std::string& id,
                                 int button_index) override;
  void ClickOnNotificationButtonWithReply(const std::string& id,
                                          int button_index,
                                          const base::string16& reply) override;
  void ClickOnSettingsButton(const std::string& id) override;
  void DisableNotification(const std::string& id) override;
  void MarkSinglePopupAsShown(const std::string& id,
                              bool mark_notification_as_read) override;
  void DisplayedNotification(const std::string& id,
                             const DisplaySource source) override;
  void SetQuietMode(bool in_quiet_mode) override;
  void EnterQuietModeWithExpire(const base::TimeDelta& expires_in) override;
  void RestartPopupTimers() override;
  void PausePopupTimers() override;
  const base::string16& GetSystemNotificationAppName() const override;
  void SetSystemNotificationAppName(const base::string16& name) override;

  // NotificationBlocker::Observer overrides:
  void OnBlockingStateChanged(NotificationBlocker* blocker) override;

  // Unexposed methods:
  void AddNotificationImmediately(std::unique_ptr<Notification> notification);
  void UpdateNotificationImmediately(
      const std::string& old_id,
      std::unique_ptr<Notification> new_notification);
  void RemoveNotificationImmediately(const std::string& id, bool by_user);

 protected:
  void DisableTimersForTest() override;

 private:
  friend internal::ScopedNotificationsIterationLock;
  class ScopedNotificationsLock;

  Notification* GetLatestNotificationIncludingQueued(
      const std::string& id) const;

  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<NotificationList> notification_list_;
  NotificationList::Notifications visible_notifications_;
  base::ObserverList<MessageCenterObserver> observer_list_;
  std::unique_ptr<PopupTimersController> popup_timers_controller_;
  std::unique_ptr<base::OneShotTimer> quiet_mode_timer_;
  std::vector<NotificationBlocker*> blockers_;
  std::unique_ptr<ChangeQueue> notification_change_queue_;

  bool visible_ = false;

  // modified by ScopedNotificationsIterationLock.
  bool iterating_ = false;

  base::string16 system_notification_app_name_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterImpl);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_H_
