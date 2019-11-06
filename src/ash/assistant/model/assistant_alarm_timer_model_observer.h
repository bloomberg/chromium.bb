// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_OBSERVER_H_

#include <map>
#include <string>

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash {

struct AlarmTimer;

// A checked observer which receives notification of changes to the Assistant
// alarm/timer model.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantAlarmTimerModelObserver
    : public base::CheckedObserver {
 public:
  // Invoked when the specified alarm/timer has been added.
  virtual void OnAlarmTimerAdded(const AlarmTimer& alarm_timer,
                                 const base::TimeDelta& time_remaining) {}

  // Invoked when the alarms/timers associated with the given ids have ticked
  // with the specified time remaining.
  virtual void OnAlarmsTimersTicked(
      const std::map<std::string, base::TimeDelta>& times_remaining) {}

  // Invoked when all alarms/timers have been removed.
  virtual void OnAllAlarmsTimersRemoved() {}

 protected:
  ~AssistantAlarmTimerModelObserver() override = default;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_OBSERVER_H_
