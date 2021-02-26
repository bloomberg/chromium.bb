// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_H_

#include <map>
#include <string>
#include <vector>

#include "ash/public/cpp/assistant/controller/assistant_alarm_timer_controller.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class AssistantAlarmTimerModelObserver;

// The model belonging to AssistantAlarmTimerController which tracks alarm/timer
// state and notifies a pool of observers.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantAlarmTimerModel {
 public:
  AssistantAlarmTimerModel();
  ~AssistantAlarmTimerModel();

  // Adds/removes the specified alarm/timer model |observer|.
  void AddObserver(AssistantAlarmTimerModelObserver* observer) const;
  void RemoveObserver(AssistantAlarmTimerModelObserver* observer) const;

  // Adds or updates the timer specified by |timer.id| in the model.
  void AddOrUpdateTimer(AssistantTimerPtr timer);

  // Removes the timer uniquely identified by |id|.
  void RemoveTimer(const std::string& id);

  // Remove all timers from the model.
  void RemoveAllTimers();

  // Returns all timers from the model.
  std::vector<const AssistantTimer*> GetAllTimers() const;

  // Returns the timer uniquely identified by |id|.
  const AssistantTimer* GetTimerById(const std::string& id) const;

  // Returns |true| if the model contains no timers, |false| otherwise.
  bool empty() const { return timers_.empty(); }

 private:
  void NotifyTimerAdded(const AssistantTimer& timer);
  void NotifyTimerUpdated(const AssistantTimer& timer);
  void NotifyTimerRemoved(const AssistantTimer& timer);

  std::map<std::string, AssistantTimerPtr> timers_;

  mutable base::ObserverList<AssistantAlarmTimerModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAlarmTimerModel);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_ALARM_TIMER_MODEL_H_
