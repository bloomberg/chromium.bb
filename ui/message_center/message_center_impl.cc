// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_impl.h"

#include "base/observer_list.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"

namespace {

base::TimeDelta GetTimeoutForPriority(int priority) {
  if (priority > message_center::DEFAULT_PRIORITY) {
    return base::TimeDelta::FromSeconds(
        message_center::kAutocloseHighPriorityDelaySeconds);
  }
  return base::TimeDelta::FromSeconds(
      message_center::kAutocloseDefaultDelaySeconds);
}

}  // namespace

namespace message_center {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// PopupTimer

PopupTimer::PopupTimer(const std::string& id,
                       base::TimeDelta timeout,
                       base::WeakPtr<PopupTimersController> controller)
    : id_(id),
      timeout_(timeout),
      timer_controller_(controller),
      timer_(new base::OneShotTimer<PopupTimersController>) {}

PopupTimer::~PopupTimer() {
  if (!timer_)
    return;

  if (timer_->IsRunning())
    timer_->Stop();
}

void PopupTimer::Start() {
  if (timer_->IsRunning())
    return;
  base::TimeDelta timeout_to_close =
      timeout_ <= passed_ ? base::TimeDelta() : timeout_ - passed_;
  start_time_ = base::Time::Now();
  timer_->Start(
      FROM_HERE,
      timeout_to_close,
      base::Bind(
          &PopupTimersController::TimerFinished, timer_controller_, id_));
}

void PopupTimer::Pause() {
  if (!timer_.get() || !timer_->IsRunning())
    return;

  timer_->Stop();
  passed_ += base::Time::Now() - start_time_;
}

void PopupTimer::Reset() {
  if (timer_)
    timer_->Stop();
  passed_ = base::TimeDelta();
}

////////////////////////////////////////////////////////////////////////////////
// PopupTimersController

PopupTimersController::PopupTimersController(MessageCenter* message_center)
    : message_center_(message_center), popup_deleter_(&popup_timers_) {
  message_center_->AddObserver(this);
}

PopupTimersController::~PopupTimersController() {
  message_center_->RemoveObserver(this);
}

void PopupTimersController::StartTimer(const std::string& id,
                                       const base::TimeDelta& timeout) {
  PopupTimerCollection::iterator iter = popup_timers_.find(id);
  if (iter != popup_timers_.end()) {
    DCHECK(iter->second);
    iter->second->Start();
    return;
  }

  PopupTimer* timer = new PopupTimer(id, timeout, AsWeakPtr());

  timer->Start();
  popup_timers_[id] = timer;
}

void PopupTimersController::StartAll() {
  std::map<std::string, PopupTimer*>::iterator iter;
  for (iter = popup_timers_.begin(); iter != popup_timers_.end(); iter++) {
    iter->second->Start();
  }
}

void PopupTimersController::ResetTimer(const std::string& id,
                                       const base::TimeDelta& timeout) {
  CancelTimer(id);
  StartTimer(id, timeout);
}

void PopupTimersController::PauseTimer(const std::string& id) {
  PopupTimerCollection::iterator iter = popup_timers_.find(id);
  if (iter == popup_timers_.end())
    return;
  iter->second->Pause();
}

void PopupTimersController::PauseAll() {
  std::map<std::string, PopupTimer*>::iterator iter;
  for (iter = popup_timers_.begin(); iter != popup_timers_.end(); iter++) {
    iter->second->Pause();
  }
}

void PopupTimersController::CancelTimer(const std::string& id) {
  PopupTimerCollection::iterator iter = popup_timers_.find(id);
  if (iter == popup_timers_.end())
    return;

  PopupTimer* timer = iter->second;
  delete timer;

  popup_timers_.erase(iter);
}

void PopupTimersController::CancelAll() {
  STLDeleteValues(&popup_timers_);
  popup_timers_.clear();
}

void PopupTimersController::TimerFinished(const std::string& id) {
  PopupTimerCollection::iterator iter = popup_timers_.find(id);
  if (iter == popup_timers_.end())
    return;

  CancelTimer(id);
  message_center_->MarkSinglePopupAsShown(id, false);
}

void PopupTimersController::OnNotificationDisplayed(const std::string& id) {
  OnNotificationUpdated(id);
}

void PopupTimersController::OnNotificationUpdated(const std::string& id) {
  NotificationList::PopupNotifications popup_notifications =
      message_center_->GetPopupNotifications();

  if (!popup_notifications.size()) {
    CancelAll();
    return;
  }

  NotificationList::PopupNotifications::const_iterator iter =
      popup_notifications.begin();
  for (; iter != popup_notifications.end(); iter++) {
    if ((*iter)->id() == id)
      break;
  }

  if (iter == popup_notifications.end() || (*iter)->never_timeout()) {
    CancelTimer(id);
    return;
  }

  // Start the timer if not yet.
  if (popup_timers_.find(id) == popup_timers_.end())
    StartTimer(id, GetTimeoutForPriority((*iter)->priority()));
}

void PopupTimersController::OnNotificationRemoved(const std::string& id,
                                                  bool by_user) {
  CancelTimer(id);
}

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// MessageCenterImpl

MessageCenterImpl::MessageCenterImpl()
    : MessageCenter(),
      popup_timers_controller_(new internal::PopupTimersController(this)),
      delegate_(NULL),
      settings_provider_(NULL) {
  notification_list_.reset(new NotificationList());
}

MessageCenterImpl::~MessageCenterImpl() {
  notification_list_.reset();
}

void MessageCenterImpl::AddObserver(MessageCenterObserver* observer) {
  observer_list_.AddObserver(observer);
}

void MessageCenterImpl::RemoveObserver(MessageCenterObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void MessageCenterImpl::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void MessageCenterImpl::SetMessageCenterVisible(bool visible) {
  std::set<std::string> updated_ids;
  notification_list_->SetMessageCenterVisible(visible, &updated_ids);
  for (std::set<std::string>::const_iterator iter = updated_ids.begin();
       iter != updated_ids.end();
       ++iter) {
    FOR_EACH_OBSERVER(
        MessageCenterObserver, observer_list_, OnNotificationUpdated(*iter));
  }

  if (!visible) {
    FOR_EACH_OBSERVER(
        MessageCenterObserver, observer_list_, OnNotificationCenterClosed());
  }
}

bool MessageCenterImpl::IsMessageCenterVisible() {
  return notification_list_->is_message_center_visible();
}

size_t MessageCenterImpl::NotificationCount() const {
  return notification_list_->NotificationCount();
}

size_t MessageCenterImpl::UnreadNotificationCount() const {
  return notification_list_->unread_count();
}

bool MessageCenterImpl::HasPopupNotifications() const {
  return notification_list_->HasPopupNotifications();
}

bool MessageCenterImpl::HasNotification(const std::string& id) {
  return notification_list_->HasNotification(id);
}

bool MessageCenterImpl::IsQuietMode() const {
  return notification_list_->quiet_mode();
}

bool MessageCenterImpl::HasClickedListener(const std::string& id) {
  NotificationDelegate* delegate =
      notification_list_->GetNotificationDelegate(id);
  return delegate && delegate->HasClickedListener();
}

const NotificationList::Notifications& MessageCenterImpl::GetNotifications() {
  return notification_list_->GetNotifications();
}

NotificationList::PopupNotifications
    MessageCenterImpl::GetPopupNotifications() {
  return notification_list_->GetPopupNotifications();
}

//------------------------------------------------------------------------------
// Client code interface.
void MessageCenterImpl::AddNotification(scoped_ptr<Notification> notification) {
  DCHECK(notification.get());

  // Sometimes the notification can be added with the same id and the
  // |notification_list| will replace the notification instead of adding new.
  // This is essentially an update rather than addition.
  const std::string& id = notification->id();
  bool already_exists = notification_list_->HasNotification(id);
  notification_list_->AddNotification(notification.Pass());

  if (already_exists) {
    FOR_EACH_OBSERVER(
        MessageCenterObserver, observer_list_, OnNotificationUpdated(id));
  } else {
    FOR_EACH_OBSERVER(
        MessageCenterObserver, observer_list_, OnNotificationAdded(id));
  }
}

void MessageCenterImpl::UpdateNotification(
    const std::string& old_id,
    scoped_ptr<Notification> new_notification) {
  std::string new_id = new_notification->id();
  notification_list_->UpdateNotificationMessage(old_id,
                                                new_notification.Pass());
  if (old_id == new_id) {
    FOR_EACH_OBSERVER(
        MessageCenterObserver, observer_list_, OnNotificationUpdated(new_id));
  } else {
    FOR_EACH_OBSERVER(MessageCenterObserver, observer_list_,
                      OnNotificationRemoved(old_id, false));
    FOR_EACH_OBSERVER(MessageCenterObserver, observer_list_,
                      OnNotificationAdded(new_id));
  }
}

void MessageCenterImpl::RemoveNotification(const std::string& id,
                                           bool by_user) {
  if (!HasNotification(id))
    return;

  NotificationDelegate* delegate =
      notification_list_->GetNotificationDelegate(id);
  if (delegate)
    delegate->Close(by_user);

  // In many cases |id| is a reference to an existing notification instance
  // but the instance can be destructed in RemoveNotification(). Hence
  // copies the id explicitly here.
  std::string copied_id(id);
  notification_list_->RemoveNotification(copied_id);
  FOR_EACH_OBSERVER(MessageCenterObserver,
                    observer_list_,
                    OnNotificationRemoved(copied_id, by_user));
}

void MessageCenterImpl::RemoveAllNotifications(bool by_user) {
  const NotificationList::Notifications& notifications =
      notification_list_->GetNotifications();
  std::set<std::string> ids;
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    ids.insert((*iter)->id());
  }
  notification_list_->RemoveAllNotifications();

  for (std::set<std::string>::const_iterator iter = ids.begin();
       iter != ids.end(); ++iter) {
    FOR_EACH_OBSERVER(MessageCenterObserver,
                      observer_list_,
                      OnNotificationRemoved(*iter, by_user));
  }
}

void MessageCenterImpl::SetNotificationIcon(const std::string& notification_id,
                                        const gfx::Image& image) {
  if (notification_list_->SetNotificationIcon(notification_id, image)) {
    FOR_EACH_OBSERVER(MessageCenterObserver, observer_list_,
                      OnNotificationUpdated(notification_id));
  }
}

void MessageCenterImpl::SetNotificationImage(const std::string& notification_id,
                                         const gfx::Image& image) {
  if (notification_list_->SetNotificationImage(notification_id, image)) {
    FOR_EACH_OBSERVER(MessageCenterObserver, observer_list_,
                      OnNotificationUpdated(notification_id));
  }
}

void MessageCenterImpl::SetNotificationButtonIcon(
    const std::string& notification_id, int button_index,
    const gfx::Image& image) {
  if (!HasNotification(notification_id))
    return;
  if (notification_list_->SetNotificationButtonIcon(notification_id,
                                                    button_index, image)) {
    FOR_EACH_OBSERVER(MessageCenterObserver, observer_list_,
                      OnNotificationUpdated(notification_id));
  }
}

void MessageCenterImpl::DisableNotificationsByExtension(
    const std::string& id) {
  if (delegate_)
    delegate_->DisableExtension(id);

  NotificationList::Notifications notifications =
      notification_list_->GetNotificationsByExtension(id);
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end();) {
    std::string id = (*iter)->id();
    iter++;
    RemoveNotification(id, false);
  }
}

void MessageCenterImpl::DisableNotificationsByUrl(const std::string& id) {
  if (delegate_)
    delegate_->DisableNotificationsFromSource(id);

  NotificationList::Notifications notifications =
      notification_list_->GetNotificationsBySource(id);
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end();) {
    std::string id = (*iter)->id();
    iter++;
    RemoveNotification(id, false);
  }
}

void MessageCenterImpl::ShowNotificationSettings(const std::string& id) {
  if (delegate_)
    delegate_->ShowSettings(id);
}

void MessageCenterImpl::ExpandNotification(const std::string& id) {
  if (!HasNotification(id))
    return;
  notification_list_->MarkNotificationAsExpanded(id);
  FOR_EACH_OBSERVER(MessageCenterObserver, observer_list_,
                    OnNotificationUpdated(id));
}

void MessageCenterImpl::ClickOnNotification(const std::string& id) {
  if (!HasNotification(id))
    return;
  if (HasPopupNotifications())
    MarkSinglePopupAsShown(id, true);
  NotificationDelegate* delegate =
      notification_list_->GetNotificationDelegate(id);
  if (delegate)
    delegate->Click();
  FOR_EACH_OBSERVER(
      MessageCenterObserver, observer_list_, OnNotificationClicked(id));
}

void MessageCenterImpl::ClickOnNotificationButton(const std::string& id,
                                              int button_index) {
  if (!HasNotification(id))
    return;
  if (HasPopupNotifications())
    MarkSinglePopupAsShown(id, true);
  NotificationDelegate* delegate =
      notification_list_->GetNotificationDelegate(id);
  if (delegate)
    delegate->ButtonClick(button_index);
  FOR_EACH_OBSERVER(
      MessageCenterObserver, observer_list_, OnNotificationButtonClicked(
          id, button_index));
}

void MessageCenterImpl::MarkSinglePopupAsShown(const std::string& id,
                                               bool mark_notification_as_read) {
  if (!HasNotification(id))
    return;
  notification_list_->MarkSinglePopupAsShown(id, mark_notification_as_read);
  FOR_EACH_OBSERVER(
      MessageCenterObserver, observer_list_, OnNotificationUpdated(id));
}

void MessageCenterImpl::DisplayedNotification(const std::string& id) {
  if (!HasNotification(id))
    return;

  if (HasPopupNotifications())
    notification_list_->MarkSinglePopupAsDisplayed(id);
  NotificationDelegate* delegate =
      notification_list_->GetNotificationDelegate(id);
  if (delegate)
    delegate->Display();
  FOR_EACH_OBSERVER(
      MessageCenterObserver, observer_list_, OnNotificationDisplayed(id));
}

void MessageCenterImpl::SetNotifierSettingsProvider(
    NotifierSettingsProvider* provider) {
  settings_provider_ = provider;
}

NotifierSettingsProvider* MessageCenterImpl::GetNotifierSettingsProvider() {
  return settings_provider_;
}

void MessageCenterImpl::SetQuietMode(bool in_quiet_mode) {
  notification_list_->SetQuietMode(in_quiet_mode);
}

void MessageCenterImpl::EnterQuietModeWithExpire(
    const base::TimeDelta& expires_in) {
  notification_list_->EnterQuietModeWithExpire(expires_in);
}

void MessageCenterImpl::RestartPopupTimers() {
  if (popup_timers_controller_.get())
    popup_timers_controller_->StartAll();
}

void MessageCenterImpl::PausePopupTimers() {
  if (popup_timers_controller_.get())
    popup_timers_controller_->PauseAll();
}

void MessageCenterImpl::DisableTimersForTest() {
  popup_timers_controller_.reset();
}

}  // namespace message_center
