// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_POPUP_TIMER_H_
#define UI_MESSAGE_CENTER_POPUP_TIMER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/notifier_settings.h"

namespace message_center {
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
  scoped_ptr<base::OneShotTimer> timer_;

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
  // Weak, global.
  MessageCenter* message_center_;

  // The PopupTimerCollection contains all the managed timers by their ID.
  using PopupTimerCollection = std::map<std::string, scoped_ptr<PopupTimer>>;
  PopupTimerCollection popup_timers_;

  DISALLOW_COPY_AND_ASSIGN(PopupTimersController);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_POPUP_TIMER_H_
