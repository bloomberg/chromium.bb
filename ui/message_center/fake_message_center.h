// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_H_
#define UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_H_

#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"

namespace message_center {

class NotificationDelegate;

// MessageCenter implementation of doing nothing. Useful for tests.
class FakeMessageCenter : public MessageCenter {
 public:
  FakeMessageCenter();
  virtual ~FakeMessageCenter();

  // Overridden from FakeMessageCenter.
  virtual void AddObserver(MessageCenterObserver* observer) override;
  virtual void RemoveObserver(MessageCenterObserver* observer) override;
  virtual void AddNotificationBlocker(NotificationBlocker* blocker) override;
  virtual void RemoveNotificationBlocker(NotificationBlocker* blocker) override;
  virtual size_t NotificationCount() const override;
  virtual size_t UnreadNotificationCount() const override;
  virtual bool HasPopupNotifications() const override;
  virtual bool IsQuietMode() const override;
  virtual bool HasClickedListener(const std::string& id) override;
  virtual message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override;
  virtual const NotificationList::Notifications& GetVisibleNotifications()
      override;
  virtual NotificationList::PopupNotifications GetPopupNotifications() override;
  virtual void AddNotification(scoped_ptr<Notification> notification) override;
  virtual void UpdateNotification(const std::string& old_id,
                                  scoped_ptr<Notification> new_notification)
      override;

  virtual void RemoveNotification(const std::string& id, bool by_user) override;
  virtual void RemoveAllNotifications(bool by_user) override;
  virtual void RemoveAllVisibleNotifications(bool by_user) override;
  virtual void SetNotificationIcon(const std::string& notification_id,
                                   const gfx::Image& image) override;

  virtual void SetNotificationImage(const std::string& notification_id,
                                    const gfx::Image& image) override;

  virtual void SetNotificationButtonIcon(const std::string& notification_id,
                                         int button_index,
                                         const gfx::Image& image) override;
  virtual void DisableNotificationsByNotifier(
      const NotifierId& notifier_id) override;
  virtual void ClickOnNotification(const std::string& id) override;
  virtual void ClickOnNotificationButton(const std::string& id,
                                         int button_index) override;
  virtual void MarkSinglePopupAsShown(const std::string& id,
                                      bool mark_notification_as_read) override;
  virtual void DisplayedNotification(
      const std::string& id,
      const DisplaySource source) override;
  virtual void SetNotifierSettingsProvider(
      NotifierSettingsProvider* provider) override;
  virtual NotifierSettingsProvider* GetNotifierSettingsProvider() override;
  virtual void SetQuietMode(bool in_quiet_mode) override;
  virtual void EnterQuietModeWithExpire(
      const base::TimeDelta& expires_in) override;
  virtual void SetVisibility(Visibility visible) override;
  virtual bool IsMessageCenterVisible() const override;
  virtual void RestartPopupTimers() override;
  virtual void PausePopupTimers() override;

 protected:
  virtual void DisableTimersForTest() override;

 private:
  const NotificationList::Notifications empty_notifications_;

  DISALLOW_COPY_AND_ASSIGN(FakeMessageCenter);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_FAKE_MESSAGE_CENTER_H_
