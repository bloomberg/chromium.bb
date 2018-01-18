// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_impl.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/popup_timers_controller.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/message_center_switches.h"

namespace message_center {

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// ScopedNotificationsIterationLock

class ScopedNotificationsIterationLock {
 public:
  // Lock may be used recursively.
  explicit ScopedNotificationsIterationLock(MessageCenterImpl* message_center)
      : message_center_(message_center),
        original_value_(message_center->iterating_) {
    message_center_->iterating_ = true;
  }

  ~ScopedNotificationsIterationLock() {
    DCHECK(message_center_->iterating_);

    // |original_value_| must be false. But handle the other case just in case.
    message_center_->iterating_ = original_value_;
    if (!original_value_) {
      message_center_->notification_change_queue_->ApplyChanges(
          message_center_);
    }
  }

 private:
  MessageCenterImpl* message_center_;
  const bool original_value_;

  DISALLOW_COPY_AND_ASSIGN(ScopedNotificationsIterationLock);
};

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// MessageCenterImpl

MessageCenterImpl::MessageCenterImpl()
    : MessageCenter(),
      popup_timers_controller_(new PopupTimersController(this)) {
  notification_list_.reset(new NotificationList(this));
  notification_change_queue_.reset(new ChangeQueue());
}

MessageCenterImpl::~MessageCenterImpl() {
}

void MessageCenterImpl::AddObserver(MessageCenterObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.AddObserver(observer);
}

void MessageCenterImpl::RemoveObserver(MessageCenterObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.RemoveObserver(observer);
}

void MessageCenterImpl::AddNotificationBlocker(NotificationBlocker* blocker) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (base::ContainsValue(blockers_, blocker))
    return;

  blocker->AddObserver(this);
  blockers_.push_back(blocker);
}

void MessageCenterImpl::RemoveNotificationBlocker(
    NotificationBlocker* blocker) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<NotificationBlocker*>::iterator iter =
      std::find(blockers_.begin(), blockers_.end(), blocker);
  if (iter == blockers_.end())
    return;
  blocker->RemoveObserver(this);
  blockers_.erase(iter);
}

void MessageCenterImpl::OnBlockingStateChanged(NotificationBlocker* blocker) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!iterating_);
  std::list<const Notification*> blocked;
  NotificationList::PopupNotifications popups =
      notification_list_->GetPopupNotifications(blockers_, &blocked);

  // Close already displayed pop-ups that are blocked now.
  for (const auto* notification : blocked) {
    // Do not call MessageCenterImpl::MarkSinglePopupAsShown() directly here
    // just for performance reason. MessageCenterImpl::MarkSinglePopupAsShown()
    // calls NotificationList::MarkSinglePopupAsShown(), but the whole cache
    // will be recreated below.
    if (notification->IsRead())
      notification_list_->MarkSinglePopupAsShown(notification->id(), true);
  }
  visible_notifications_ =
      notification_list_->GetVisibleNotifications(blockers_);

  {
    internal::ScopedNotificationsIterationLock lock(this);
    for (const auto* notification : blocked) {
      for (auto& observer : observer_list_)
        observer.OnNotificationUpdated(notification->id());
    }
  }
  for (auto& observer : observer_list_)
    observer.OnBlockingStateChanged(blocker);
}

void MessageCenterImpl::SetVisibility(Visibility visibility) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!iterating_);
  visible_ = (visibility == VISIBILITY_MESSAGE_CENTER);

  if (visible_) {
    std::set<std::string> updated_ids;
    notification_list_->SetNotificationsShown(blockers_, &updated_ids);

    {
      internal::ScopedNotificationsIterationLock lock(this);
      for (const auto& id : updated_ids) {
        for (auto& observer : observer_list_)
          observer.OnNotificationUpdated(id);
      }
    }
  }

  for (auto& observer : observer_list_)
    observer.OnCenterVisibilityChanged(visibility);
}

bool MessageCenterImpl::IsMessageCenterVisible() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return visible_;
}

size_t MessageCenterImpl::NotificationCount() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return visible_notifications_.size();
}

bool MessageCenterImpl::HasPopupNotifications() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !IsMessageCenterVisible() &&
      notification_list_->HasPopupNotifications(blockers_);
}

bool MessageCenterImpl::IsQuietMode() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return notification_list_->quiet_mode();
}

message_center::Notification* MessageCenterImpl::FindVisibleNotificationById(
    const std::string& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return notification_list_->GetNotificationById(id);
}

const NotificationList::Notifications&
MessageCenterImpl::GetVisibleNotifications() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return visible_notifications_;
}

NotificationList::PopupNotifications
    MessageCenterImpl::GetPopupNotifications() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return notification_list_->GetPopupNotifications(blockers_, NULL);
}

//------------------------------------------------------------------------------
// Client code interface.
void MessageCenterImpl::AddNotification(
    std::unique_ptr<Notification> notification) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(notification);
  const std::string id = notification->id();
  for (size_t i = 0; i < blockers_.size(); ++i)
    blockers_[i]->CheckState();

  if (iterating_) {
    notification_change_queue_->AddNotification(std::move(notification));
    return;
  }

  AddNotificationImmediately(std::move(notification));
}

void MessageCenterImpl::AddNotificationImmediately(
    std::unique_ptr<Notification> notification) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!iterating_);
  const std::string id = notification->id();

  // Sometimes the notification can be added with the same id and the
  // |notification_list| will replace the notification instead of adding new.
  // This is essentially an update rather than addition.
  bool already_exists = (notification_list_->GetNotificationById(id) != NULL);
  notification_list_->AddNotification(std::move(notification));
  visible_notifications_ =
      notification_list_->GetVisibleNotifications(blockers_);

  if (already_exists) {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationUpdated(id);
  } else {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationAdded(id);
  }
}

void MessageCenterImpl::UpdateNotification(
    const std::string& old_id,
    std::unique_ptr<Notification> new_notification) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (size_t i = 0; i < blockers_.size(); ++i)
    blockers_[i]->CheckState();

  if (iterating_) {
    notification_change_queue_->UpdateNotification(old_id,
                                                   std::move(new_notification));
    return;
  }

  UpdateNotificationImmediately(old_id, std::move(new_notification));
}

void MessageCenterImpl::UpdateNotificationImmediately(
    const std::string& old_id,
    std::unique_ptr<Notification> new_notification) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!iterating_);
  std::string new_id = new_notification->id();
  notification_list_->UpdateNotificationMessage(old_id,
                                                std::move(new_notification));
  visible_notifications_ =
      notification_list_->GetVisibleNotifications(blockers_);
  if (old_id == new_id) {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationUpdated(new_id);
  } else {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationRemoved(old_id, false);
    for (auto& observer : observer_list_)
      observer.OnNotificationAdded(new_id);
  }
}

void MessageCenterImpl::RemoveNotification(const std::string& id,
                                           bool by_user) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (iterating_) {
    notification_change_queue_->RemoveNotification(id, by_user);
    return;
  }

  RemoveNotificationImmediately(id, by_user);
}

void MessageCenterImpl::RemoveNotificationImmediately(
    const std::string& id, bool by_user) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!iterating_);

  Notification* notification = FindVisibleNotificationById(id);
  if (notification == NULL)
    return;

  if (by_user && notification->pinned()) {
    // When pinned, a popup will not be removed completely but moved into the
    // message center bubble.
    MarkSinglePopupAsShown(id, true);
    return;
  }

  // In many cases |id| is a reference to an existing notification instance
  // but the instance can be destructed in this method. Hence copies the id
  // explicitly here.
  std::string copied_id(id);

  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(copied_id);
  if (delegate.get())
    delegate->Close(by_user);

  notification_list_->RemoveNotification(copied_id);
  visible_notifications_ =
      notification_list_->GetVisibleNotifications(blockers_);
  {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationRemoved(copied_id, by_user);
  }
}

Notification* MessageCenterImpl::GetLatestNotificationIncludingQueued(
    const std::string& id) const {
  Notification* queued_notification =
      notification_change_queue_->GetLatestNotification(id);
  if (queued_notification) {
    DCHECK(iterating_);
    return queued_notification;
  }

  Notification* notification = notification_list_->GetNotificationById(id);
  if (notification)
    return notification;

  return nullptr;
}

void MessageCenterImpl::RemoveNotificationsForNotifierId(
    const NotifierId& notifier_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NotificationList::Notifications notifications =
      notification_list_->GetNotificationsByNotifierId(notifier_id);
  for (auto* notification : notifications)
    RemoveNotification(notification->id(), false);
  if (!notifications.empty()) {
    visible_notifications_ =
        notification_list_->GetVisibleNotifications(blockers_);
  }
}

void MessageCenterImpl::RemoveAllNotifications(bool by_user, RemoveType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!iterating_);
  bool remove_pinned = (type == RemoveType::ALL);

  const NotificationBlockers& blockers =
      remove_pinned ? NotificationBlockers() /* empty blockers */
                    : blockers_;             /* use default blockers */

  const NotificationList::Notifications notifications =
      notification_list_->GetVisibleNotifications(blockers);
  std::set<std::string> ids;
  for (auto* notification : notifications) {
    if (!remove_pinned && notification->pinned())
      continue;

    ids.insert(notification->id());
    scoped_refptr<NotificationDelegate> delegate = notification->delegate();
    if (delegate.get())
      delegate->Close(by_user);
    notification_list_->RemoveNotification(notification->id());
  }

  if (!ids.empty()) {
    visible_notifications_ =
        notification_list_->GetVisibleNotifications(blockers_);
  }
  {
    internal::ScopedNotificationsIterationLock lock(this);
    for (const auto& id : ids) {
      for (auto& observer : observer_list_)
        observer.OnNotificationRemoved(id, by_user);
    }
  }
}

void MessageCenterImpl::SetNotificationIcon(const std::string& notification_id,
                                            const gfx::Image& image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (iterating_) {
    Notification* notification =
        GetLatestNotificationIncludingQueued(notification_id);
    if (notification) {
      std::unique_ptr<Notification> copied_notification =
          std::make_unique<Notification>(*notification);
      copied_notification->set_icon(image);
      notification_change_queue_->UpdateNotification(
          notification_id, std::move(copied_notification));
    }
    return;
  }

  if (notification_list_->SetNotificationIcon(notification_id, image)) {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationUpdated(notification_id);
  }
}

void MessageCenterImpl::SetNotificationImage(const std::string& notification_id,
                                             const gfx::Image& image) {
  if (iterating_) {
    Notification* notification =
        GetLatestNotificationIncludingQueued(notification_id);
    if (notification) {
      std::unique_ptr<Notification> copied_notification =
          std::make_unique<Notification>(*notification);
      copied_notification->set_image(image);
      notification_change_queue_->UpdateNotification(
          notification_id, std::move(copied_notification));
    }
    return;
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (notification_list_->SetNotificationImage(notification_id, image)) {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationUpdated(notification_id);
  }
}

void MessageCenterImpl::SetNotificationButtonIcon(
    const std::string& notification_id, int button_index,
    const gfx::Image& image) {
  if (iterating_) {
    Notification* notification =
        GetLatestNotificationIncludingQueued(notification_id);
    if (notification) {
      std::unique_ptr<Notification> copied_notification =
          std::make_unique<Notification>(*notification);
      copied_notification->SetButtonIcon(button_index, image);
      notification_change_queue_->UpdateNotification(
          notification_id, std::move(copied_notification));
    }
    return;
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (notification_list_->SetNotificationButtonIcon(notification_id,
                                                    button_index, image)) {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationUpdated(notification_id);
  }
}

void MessageCenterImpl::ClickOnNotification(const std::string& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!iterating_);
  if (FindVisibleNotificationById(id) == NULL)
    return;
#if defined(OS_CHROMEOS)
  if (HasPopupNotifications())
    MarkSinglePopupAsShown(id, true);
#endif
  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(id);
  if (delegate.get())
    delegate->Click();
  {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationClicked(id);
  }
}

void MessageCenterImpl::ClickOnNotificationButton(const std::string& id,
                                                  int button_index) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!iterating_);
  if (!FindVisibleNotificationById(id))
    return;
#if defined(OS_CHROMEOS)
  if (HasPopupNotifications())
    MarkSinglePopupAsShown(id, true);
#endif
  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(id);
  if (delegate.get())
    delegate->ButtonClick(button_index);
  {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationButtonClicked(id, button_index);
  }
}

void MessageCenterImpl::ClickOnNotificationButtonWithReply(
    const std::string& id,
    int button_index,
    const base::string16& reply) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!FindVisibleNotificationById(id))
    return;
#if defined(OS_CHROMEOS)
  if (HasPopupNotifications())
    MarkSinglePopupAsShown(id, true);
#endif
  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(id);
  if (delegate.get())
    delegate->ButtonClickWithReply(button_index, reply);
  {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationButtonClickedWithReply(id, button_index, reply);
  }
}

void MessageCenterImpl::ClickOnSettingsButton(const std::string& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!iterating_);
  Notification* notification = notification_list_->GetNotificationById(id);

  bool handled_by_delegate =
      notification &&
      (notification->rich_notification_data().settings_button_handler ==
       SettingsButtonHandler::DELEGATE);
  if (handled_by_delegate)
    notification->delegate()->SettingsClick();

  {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationSettingsClicked(handled_by_delegate);
  }
}

void MessageCenterImpl::DisableNotification(const std::string& id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!iterating_);
  Notification* notification = notification_list_->GetNotificationById(id);

  if (notification && notification->delegate()) {
    notification->delegate()->DisableNotification();
    RemoveNotificationsForNotifierId(notification->notifier_id());
  }
}

void MessageCenterImpl::MarkSinglePopupAsShown(const std::string& id,
                                               bool mark_notification_as_read) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (FindVisibleNotificationById(id) == NULL)
    return;

// This method doesn't check the |iterating_| flag, since this is called
// during iteration.
// TODO(yoshiki): Investigate and not to call this method during iteration if
// necessary.

#if !defined(OS_CHROMEOS)
  RemoveNotification(id, false);
#else
  notification_list_->MarkSinglePopupAsShown(id, mark_notification_as_read);
  {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationUpdated(id);
  }
#endif  // defined(OS_CHROMEOS)
}

void MessageCenterImpl::DisplayedNotification(
    const std::string& id,
    const DisplaySource source) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This method may be called from the handlers, so we shouldn't manipulate
  // notifications in this method.

  if (FindVisibleNotificationById(id) == NULL)
    return;

  if (HasPopupNotifications())
    notification_list_->MarkSinglePopupAsDisplayed(id);
  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(id);
  {
    internal::ScopedNotificationsIterationLock lock(this);
    for (auto& observer : observer_list_)
      observer.OnNotificationDisplayed(id, source);
  }
}

void MessageCenterImpl::SetQuietMode(bool in_quiet_mode) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (in_quiet_mode != notification_list_->quiet_mode()) {
    notification_list_->SetQuietMode(in_quiet_mode);
    for (auto& observer : observer_list_)
      observer.OnQuietModeChanged(in_quiet_mode);
  }
  quiet_mode_timer_.reset();
}

void MessageCenterImpl::EnterQuietModeWithExpire(
    const base::TimeDelta& expires_in) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (quiet_mode_timer_) {
    // Note that the capital Reset() is the method to restart the timer, not
    // scoped_ptr::reset().
    quiet_mode_timer_->Reset();
  } else {
    notification_list_->SetQuietMode(true);
    for (auto& observer : observer_list_)
      observer.OnQuietModeChanged(true);

    quiet_mode_timer_.reset(new base::OneShotTimer);
    quiet_mode_timer_->Start(
        FROM_HERE,
        expires_in,
        base::Bind(
            &MessageCenterImpl::SetQuietMode, base::Unretained(this), false));
  }
}

void MessageCenterImpl::RestartPopupTimers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (popup_timers_controller_)
    popup_timers_controller_->StartAll();
}

void MessageCenterImpl::PausePopupTimers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (popup_timers_controller_)
    popup_timers_controller_->PauseAll();
}

const base::string16& MessageCenterImpl::GetSystemNotificationAppName() const {
  return system_notification_app_name_;
}

void MessageCenterImpl::SetSystemNotificationAppName(
    const base::string16& name) {
  system_notification_app_name_ = name;
}

void MessageCenterImpl::DisableTimersForTest() {
  popup_timers_controller_.reset();
}

}  // namespace message_center
