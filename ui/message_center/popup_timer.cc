// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/popup_timer.h"

#include <algorithm>

#include "base/stl_util.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"

namespace message_center {

namespace {

base::TimeDelta GetTimeoutForNotification(Notification* notification) {
  if (notification->priority() > DEFAULT_PRIORITY)
    return base::TimeDelta::FromSeconds(kAutocloseHighPriorityDelaySeconds);
  if (notification->notifier_id().type == NotifierId::WEB_PAGE)
    return base::TimeDelta::FromSeconds(kAutocloseWebPageDelaySeconds);
  return base::TimeDelta::FromSeconds(kAutocloseDefaultDelaySeconds);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PopupTimer

PopupTimer::PopupTimer(const std::string& id,
                       base::TimeDelta timeout,
                       base::WeakPtr<PopupTimersController> controller)
    : id_(id),
      timeout_(timeout),
      timer_controller_(controller),
      timer_(new base::OneShotTimer) {}

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
  timer_->Start(FROM_HERE, timeout_to_close,
                base::Bind(&PopupTimersController::TimerFinished,
                           timer_controller_, id_));
}

void PopupTimer::Pause() {
  if (!timer_ || !timer_->IsRunning())
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
    : message_center_(message_center) {
  message_center_->AddObserver(this);
}

PopupTimersController::~PopupTimersController() {
  message_center_->RemoveObserver(this);
}

void PopupTimersController::StartTimer(const std::string& id,
                                       const base::TimeDelta& timeout) {
  PopupTimerCollection::const_iterator iter = popup_timers_.find(id);
  if (iter != popup_timers_.end()) {
    DCHECK(iter->second);
    iter->second->Start();
    return;
  }

  scoped_ptr<PopupTimer> timer(new PopupTimer(id, timeout, AsWeakPtr()));

  timer->Start();
  popup_timers_.insert(std::make_pair(id, std::move(timer)));
}

void PopupTimersController::StartAll() {
  for (const auto& iter : popup_timers_)
    iter.second->Start();
}

void PopupTimersController::ResetTimer(const std::string& id,
                                       const base::TimeDelta& timeout) {
  CancelTimer(id);
  StartTimer(id, timeout);
}

void PopupTimersController::PauseTimer(const std::string& id) {
  PopupTimerCollection::const_iterator iter = popup_timers_.find(id);
  if (iter == popup_timers_.end())
    return;
  iter->second->Pause();
}

void PopupTimersController::PauseAll() {
  for (const auto& iter : popup_timers_)
    iter.second->Pause();
}

void PopupTimersController::CancelTimer(const std::string& id) {
  popup_timers_.erase(id);
}

void PopupTimersController::CancelAll() {
  popup_timers_.clear();
}

void PopupTimersController::TimerFinished(const std::string& id) {
  if (!ContainsKey(popup_timers_, id))
    return;

  CancelTimer(id);
  message_center_->MarkSinglePopupAsShown(id, false);
}

void PopupTimersController::OnNotificationDisplayed(
    const std::string& id,
    const DisplaySource source) {
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
  for (; iter != popup_notifications.end(); ++iter) {
    if ((*iter)->id() == id)
      break;
  }

  if (iter == popup_notifications.end() || (*iter)->never_timeout()) {
    CancelTimer(id);
    return;
  }

  // Start the timer if not yet.
  if (popup_timers_.find(id) == popup_timers_.end())
    StartTimer(id, GetTimeoutForNotification(*iter));
}

void PopupTimersController::OnNotificationRemoved(const std::string& id,
                                                  bool by_user) {
  CancelTimer(id);
}

}  // namespace message_center
