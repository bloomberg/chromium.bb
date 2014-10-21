// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_IMPL_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/notifier_settings.h"

namespace message_center {
class NotificationDelegate;
class MessageCenterImpl;

namespace internal {
class ChangeQueue;
class PopupTimersController;

// A class that manages timeout behavior for notification popups.  One instance
// is created per notification popup.
class PopupTimer {
 public:
  // Accepts a notification ID, time until callback, and a reference to the
  // controller which will be called back.  The reference is a weak pointer so
  // that timers never cause a callback on a destructed object.
  PopupTimer(const std::string& id,
             base::TimeDelta timeout,
             base::WeakPtr<PopupTimersController> controller);
  ~PopupTimer();

  // Starts running the timer.  Barring a Pause or Reset call, the timer will
  // call back to |controller| after |timeout| seconds.
  void Start();

  // Stops the timer, and retains the amount of time that has passed so that on
  // subsequent calls to Start the timer will continue where it left off.
  void Pause();

  // Stops the timer, and resets the amount of time that has passed so that
  // calling Start results in a timeout equal to the initial timeout setting.
  void Reset();

  base::TimeDelta get_timeout() const { return timeout_; }

 private:
  // Notification ID for which this timer applies.
  const std::string id_;

  // Total time that should pass while active before calling TimerFinished.
  base::TimeDelta timeout_;

  // If paused, the amount of time that passed before pause.
  base::TimeDelta passed_;

  // The time that the timer was last started.
  base::Time start_time_;

  // Callback recipient.
  base::WeakPtr<PopupTimersController> timer_controller_;

  // The actual timer.
  scoped_ptr<base::OneShotTimer<PopupTimersController> > timer_;

  DISALLOW_COPY_AND_ASSIGN(PopupTimer);
};

// A class that manages all the timers running for individual notification popup
// windows.  It supports weak pointers in order to allow safe callbacks when
// timers expire.
class MESSAGE_CENTER_EXPORT PopupTimersController
    : public base::SupportsWeakPtr<PopupTimersController>,
      public MessageCenterObserver {
 public:
  explicit PopupTimersController(MessageCenter* message_center);
  ~PopupTimersController() override;

  // MessageCenterObserver implementation.
  void OnNotificationDisplayed(const std::string& id,
                               const DisplaySource source) override;
  void OnNotificationUpdated(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;

  // Callback for each timer when its time is up.
  virtual void TimerFinished(const std::string& id);

  // Pauses all running timers.
  void PauseAll();

  // Continues all managed timers.
  void StartAll();

  // Removes all managed timers.
  void CancelAll();

  // Starts a timer (by creating a PopupTimer) for |id|.
  void StartTimer(const std::string& id,
                  const base::TimeDelta& timeout_in_seconds);

  // Stops a single timer, reverts it to a new timeout, and restarts it.
  void ResetTimer(const std::string& id,
                  const base::TimeDelta& timeout_in_seconds);

  // Pauses a single timer, such that it will continue where it left off after a
  // call to StartAll or StartTimer.
  void PauseTimer(const std::string& id);

  // Removes and cancels a single popup timer, if it exists.
  void CancelTimer(const std::string& id);

 private:
  // Weak, this class is owned by MessageCenterImpl.
  MessageCenter* message_center_;

  // The PopupTimerCollection contains all the managed timers by their ID.  They
  // are owned by this class, and deleted by |popup_deleter_| on destructon or
  // when explicitly cancelled.
  typedef std::map<std::string, PopupTimer*> PopupTimerCollection;
  PopupTimerCollection popup_timers_;
  STLValueDeleter<PopupTimerCollection> popup_deleter_;

  DISALLOW_COPY_AND_ASSIGN(PopupTimersController);
};

}  // namespace internal

// The default implementation of MessageCenter.
class MessageCenterImpl : public MessageCenter,
                          public NotificationBlocker::Observer,
                          public message_center::NotifierSettingsObserver {
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
  size_t UnreadNotificationCount() const override;
  bool HasPopupNotifications() const override;
  bool IsQuietMode() const override;
  bool HasClickedListener(const std::string& id) override;
  message_center::Notification* FindVisibleNotificationById(
      const std::string& id) override;
  const NotificationList::Notifications& GetVisibleNotifications() override;
  NotificationList::PopupNotifications GetPopupNotifications() override;
  void AddNotification(scoped_ptr<Notification> notification) override;
  void UpdateNotification(const std::string& old_id,
                          scoped_ptr<Notification> new_notification) override;
  void RemoveNotification(const std::string& id, bool by_user) override;
  void RemoveAllNotifications(bool by_user) override;
  void RemoveAllVisibleNotifications(bool by_user) override;
  void SetNotificationIcon(const std::string& notification_id,
                           const gfx::Image& image) override;
  void SetNotificationImage(const std::string& notification_id,
                            const gfx::Image& image) override;
  void SetNotificationButtonIcon(const std::string& notification_id,
                                 int button_index,
                                 const gfx::Image& image) override;
  void DisableNotificationsByNotifier(const NotifierId& notifier_id) override;
  void ClickOnNotification(const std::string& id) override;
  void ClickOnNotificationButton(const std::string& id,
                                 int button_index) override;
  void MarkSinglePopupAsShown(const std::string& id,
                              bool mark_notification_as_read) override;
  void DisplayedNotification(const std::string& id,
                             const DisplaySource source) override;
  void SetNotifierSettingsProvider(NotifierSettingsProvider* provider) override;
  NotifierSettingsProvider* GetNotifierSettingsProvider() override;
  void SetQuietMode(bool in_quiet_mode) override;
  void EnterQuietModeWithExpire(const base::TimeDelta& expires_in) override;
  void RestartPopupTimers() override;
  void PausePopupTimers() override;

  // NotificationBlocker::Observer overrides:
  void OnBlockingStateChanged(NotificationBlocker* blocker) override;

  // message_center::NotifierSettingsObserver overrides:
  void UpdateIconImage(const NotifierId& notifier_id,
                       const gfx::Image& icon) override;
  void NotifierGroupChanged() override;
  void NotifierEnabledChanged(const NotifierId& notifier_id,
                              bool enabled) override;

 protected:
  void DisableTimersForTest() override;

 private:
  struct NotificationCache {
    NotificationCache();
    ~NotificationCache();
    void Rebuild(const NotificationList::Notifications& notifications);
    void RecountUnread();

    NotificationList::Notifications visible_notifications;
    size_t unread_count;
  };

  void RemoveNotifications(bool by_user, const NotificationBlockers& blockers);
  void RemoveNotificationsForNotifierId(const NotifierId& notifier_id);

  scoped_ptr<NotificationList> notification_list_;
  NotificationCache notification_cache_;
  ObserverList<MessageCenterObserver> observer_list_;
  scoped_ptr<internal::PopupTimersController> popup_timers_controller_;
  scoped_ptr<base::OneShotTimer<MessageCenterImpl> > quiet_mode_timer_;
  NotifierSettingsProvider* settings_provider_;
  std::vector<NotificationBlocker*> blockers_;

  // Queue for the notifications to delay the addition/updates when the message
  // center is visible.
  scoped_ptr<internal::ChangeQueue> notification_queue_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterImpl);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_H_
