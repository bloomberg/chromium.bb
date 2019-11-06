// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/background_task_coordinator.h"

#include <algorithm>
#include <utility>

#include "base/numerics/ranges.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "chrome/browser/notifications/scheduler/impression_types.h"
#include "chrome/browser/notifications/scheduler/notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/scheduler_utils.h"

namespace notifications {
namespace {

class BackgroundTaskCoordinatorHelper {
 public:
  BackgroundTaskCoordinatorHelper(
      NotificationBackgroundTaskScheduler* background_task,
      const SchedulerConfig* config,
      base::Clock* clock)
      : background_task_(background_task), config_(config), clock_(clock) {}
  ~BackgroundTaskCoordinatorHelper() = default;

  void ScheduleBackgroundTask(
      BackgroundTaskCoordinator::Notifications notifications,
      BackgroundTaskCoordinator::ClientStates client_states,
      SchedulerTaskTime task_start_time) {
    if (notifications.empty()) {
      background_task_->Cancel();
      return;
    }

    std::map<SchedulerClientType, int> shown_per_type;
    int shown_total = 0;
    SchedulerClientType last_shown_type = SchedulerClientType::kUnknown;
    NotificationsShownToday(client_states, &shown_per_type, &shown_total,
                            &last_shown_type);
    bool reach_max_today_all_type =
        shown_total >= config_->max_daily_shown_all_type;
    base::Time next_morning = NextMorning();
    base::Time this_evening = ThisEvening();

    for (const auto& pair : notifications) {
      auto type = pair.first;
      auto it = client_states.find(type);
      if (pair.second.empty() || (it == client_states.end()))
        continue;

      const ClientState* client_state = it->second;

      // Try to schedule on the day that suppression expires.
      if (client_state->suppression_info.has_value()) {
        const auto& suppression = client_state->suppression_info.value();
        base::Time suppression_expire;
        ToLocalHour(config_->morning_task_hour, suppression.ReleaseTime(),
                    0 /*day_delta*/, &suppression_expire);
        MaybeUpdateBackgroundTaskTime(
            std::max(suppression_expire, next_morning));
        continue;
      }

      // Has met the quota for this notification type or for all types, only can
      // send more on next day.
      bool reach_max_today =
          shown_per_type[type] >= client_state->current_max_daily_show;
      if (reach_max_today || reach_max_today_all_type) {
        MaybeUpdateBackgroundTaskTime(next_morning);
        continue;
      }

      switch (task_start_time) {
        case SchedulerTaskTime::kMorning:
          // Still can send more in the evening.
          MaybeUpdateBackgroundTaskTime(this_evening);
          break;
        case SchedulerTaskTime::kEvening:
          // Wait until the next calendar day.
          MaybeUpdateBackgroundTaskTime(next_morning);
          break;
        case SchedulerTaskTime::kUnknown:
          // TODO(xingliu): Support arbitrary time background task.
          NOTIMPLEMENTED();
          break;
      }
    }

    ScheduleBackgroundTaskInternal(task_start_time);
  }

 private:
  // Returns the morning background task time on the next day.
  base::Time NextMorning() {
    base::Time next_morning;
    bool success = ToLocalHour(config_->morning_task_hour, clock_->Now(),
                               1 /*day_delta*/, &next_morning);
    DCHECK(success);
    return next_morning;
  }

  // Returns the evening background task time on today.
  base::Time ThisEvening() {
    base::Time this_evening;
    bool success = ToLocalHour(config_->evening_task_hour, clock_->Now(),
                               0 /*day_delta*/, &this_evening);
    DCHECK(success);
    return this_evening;
  }

  void MaybeUpdateBackgroundTaskTime(const base::Time& time) {
    if (!background_task_time_.has_value() ||
        time < background_task_time_.value())
      background_task_time_ = time;
  }

  void ScheduleBackgroundTaskInternal(SchedulerTaskTime task_start_time) {
    if (!background_task_time_.has_value())
      return;

    base::TimeDelta window_start_time =
        background_task_time_.value() - clock_->Now();
    window_start_time = base::ClampToRange(window_start_time, base::TimeDelta(),
                                           base::TimeDelta::Max());

    background_task_->Schedule(
        task_start_time, window_start_time,
        window_start_time + config_->background_task_window_duration);
  }

  NotificationBackgroundTaskScheduler* background_task_;
  const SchedulerConfig* config_;
  base::Clock* clock_;
  base::Optional<base::Time> background_task_time_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundTaskCoordinatorHelper);
};

}  // namespace

BackgroundTaskCoordinator::BackgroundTaskCoordinator(
    std::unique_ptr<NotificationBackgroundTaskScheduler> background_task,
    const SchedulerConfig* config,
    base::Clock* clock)
    : background_task_(std::move(background_task)),
      config_(config),
      clock_(clock) {}

BackgroundTaskCoordinator::~BackgroundTaskCoordinator() = default;

void BackgroundTaskCoordinator::ScheduleBackgroundTask(
    Notifications notifications,
    ClientStates client_states,
    SchedulerTaskTime task_start_time) {
  auto helper = std::make_unique<BackgroundTaskCoordinatorHelper>(
      background_task_.get(), config_, clock_);
  helper->ScheduleBackgroundTask(std::move(notifications),
                                 std::move(client_states), task_start_time);
}

}  // namespace notifications
