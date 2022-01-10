// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "ash/assistant/model/assistant_alarm_timer_model.h"
#include "ash/assistant/model/assistant_alarm_timer_model_observer.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_alarm_timer_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"

namespace ash {

namespace assistant {
namespace util {
enum class AlarmTimerAction;
}  // namespace util
}  // namespace assistant

class AssistantControllerImpl;

// The AssistantAlarmTimerControllerImpl is a sub-controller of
// AssistantController tasked with tracking alarm/timer state and providing
// alarm/timer APIs.
class AssistantAlarmTimerControllerImpl
    : public AssistantAlarmTimerController,
      public AssistantControllerObserver,
      public AssistantStateObserver,
      public AssistantAlarmTimerModelObserver {
 public:
  explicit AssistantAlarmTimerControllerImpl(
      AssistantControllerImpl* assistant_controller);

  AssistantAlarmTimerControllerImpl(const AssistantAlarmTimerControllerImpl&) =
      delete;
  AssistantAlarmTimerControllerImpl& operator=(
      const AssistantAlarmTimerControllerImpl&) = delete;

  ~AssistantAlarmTimerControllerImpl() override;

  // Provides a pointer to the |assistant| owned by AssistantService.
  void SetAssistant(chromeos::assistant::Assistant* assistant);

  // AssistantAlarmTimerController:
  const AssistantAlarmTimerModel* GetModel() const override;
  void OnTimerStateChanged(
      const std::vector<chromeos::assistant::AssistantTimer>& timers) override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // AssistantStateObserver:
  void OnAssistantStatusChanged(
      chromeos::assistant::AssistantStatus status) override;

  // AssistantAlarmTimerModelObserver:
  void OnTimerAdded(const chromeos::assistant::AssistantTimer& timer) override;
  void OnTimerUpdated(
      const chromeos::assistant::AssistantTimer& timer) override;
  void OnTimerRemoved(
      const chromeos::assistant::AssistantTimer& timer) override;

 private:
  void PerformAlarmTimerAction(const assistant::util::AlarmTimerAction& action,
                               const std::string& alarm_timer_id,
                               const absl::optional<base::TimeDelta>& duration);

  void ScheduleNextTick(const chromeos::assistant::AssistantTimer& timer);
  void Tick(const std::string& timer_id);

  AssistantControllerImpl* const assistant_controller_;  // Owned by Shell.

  AssistantAlarmTimerModel model_;

  // We independently tick timers in our |model_| to update their respective
  // remaining times. This map contains these tickers, keyed by timer id.
  std::map<std::string, base::OneShotTimer> tickers_;

  // Owned by AssistantService.
  chromeos::assistant::Assistant* assistant_;

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_ALARM_TIMER_CONTROLLER_IMPL_H_
