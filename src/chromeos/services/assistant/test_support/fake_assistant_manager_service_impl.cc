// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/test_support/fake_assistant_manager_service_impl.h"

#include <utility>

namespace chromeos {
namespace assistant {

FakeAssistantManagerServiceImpl::FakeAssistantManagerServiceImpl() = default;

FakeAssistantManagerServiceImpl::~FakeAssistantManagerServiceImpl() = default;

void FakeAssistantManagerServiceImpl::FinishStart() {
  SetStateAndInformObservers(State::RUNNING);
}

void FakeAssistantManagerServiceImpl::Start(
    const absl::optional<UserInfo>& user,
    bool enable_hotword) {
  SetStateAndInformObservers(State::STARTING);
  SetUser(user);
}

void FakeAssistantManagerServiceImpl::Stop() {
  SetStateAndInformObservers(State::STOPPED);
}

void FakeAssistantManagerServiceImpl::SetUser(
    const absl::optional<UserInfo>& user) {
  if (user) {
    gaia_id_ = user.value().gaia_id;
    access_token_ = user.value().access_token;
  } else {
    gaia_id_ = absl::nullopt;
    access_token_ = absl::nullopt;
  }
}

void FakeAssistantManagerServiceImpl::EnableListening(bool enable) {}

void FakeAssistantManagerServiceImpl::EnableHotword(bool enable) {}

void FakeAssistantManagerServiceImpl::SetArcPlayStoreEnabled(bool enabled) {}

void FakeAssistantManagerServiceImpl::SetAssistantContextEnabled(bool enabled) {
}

AssistantManagerService::State FakeAssistantManagerServiceImpl::GetState()
    const {
  return state_;
}

AssistantSettings* FakeAssistantManagerServiceImpl::GetAssistantSettings() {
  return &assistant_settings_;
}

void FakeAssistantManagerServiceImpl::AddAndFireStateObserver(
    StateObserver* observer) {
  state_observers_.AddObserver(observer);
  observer->OnStateChanged(GetState());
}

void FakeAssistantManagerServiceImpl::RemoveStateObserver(
    const StateObserver* observer) {
  state_observers_.RemoveObserver(observer);
}

void FakeAssistantManagerServiceImpl::UpdateInternalMediaPlayerStatus(
    MediaSessionAction action) {}

void FakeAssistantManagerServiceImpl::StartEditReminderInteraction(
    const std::string& client_id) {}

void FakeAssistantManagerServiceImpl::StartScreenContextInteraction(
    ax::mojom::AssistantStructurePtr assistant_structure,
    const std::vector<uint8_t>& assistant_screenshot) {}

void FakeAssistantManagerServiceImpl::StartTextInteraction(
    const std::string& query,
    AssistantQuerySource source,
    bool allow_tts) {}

void FakeAssistantManagerServiceImpl::StartVoiceInteraction() {}

void FakeAssistantManagerServiceImpl::StopActiveInteraction(
    bool cancel_conversation) {}

void FakeAssistantManagerServiceImpl::AddAssistantInteractionSubscriber(
    AssistantInteractionSubscriber* subscriber) {}

void FakeAssistantManagerServiceImpl::RemoveAssistantInteractionSubscriber(
    AssistantInteractionSubscriber* subscriber) {}

mojo::PendingReceiver<chromeos::libassistant::mojom::NotificationDelegate>
FakeAssistantManagerServiceImpl::GetPendingNotificationDelegate() {
  return mojo::PendingReceiver<
      chromeos::libassistant::mojom::NotificationDelegate>();
}

void FakeAssistantManagerServiceImpl::RetrieveNotification(
    const AssistantNotification& notification,
    int action_index) {}

void FakeAssistantManagerServiceImpl::DismissNotification(
    const AssistantNotification& notification) {}

void FakeAssistantManagerServiceImpl::OnAccessibilityStatusChanged(
    bool spoken_feedback_enabled) {}

void FakeAssistantManagerServiceImpl::OnColorModeChanged(
    bool dark_mode_enabled) {}

void FakeAssistantManagerServiceImpl::SendAssistantFeedback(
    const AssistantFeedback& feedback) {}

void FakeAssistantManagerServiceImpl::AddTimeToTimer(const std::string& id,
                                                     base::TimeDelta duration) {
}

void FakeAssistantManagerServiceImpl::PauseTimer(const std::string& id) {}

void FakeAssistantManagerServiceImpl::RemoveAlarmOrTimer(
    const std::string& id) {}

void FakeAssistantManagerServiceImpl::ResumeTimer(const std::string& id) {}

void FakeAssistantManagerServiceImpl::SetStateAndInformObservers(
    State new_state) {
  State old_state = state_;
  state_ = new_state;

  // In reality we will not skip states, i.e. we will always get |STARTING|
  // before ever encountering |STARTED|. As such our fake implementation will
  // send out all intermediate states between |old_state| and |new_state|.
  MaybeSendStateChange(State::STOPPED, old_state, new_state);
  MaybeSendStateChange(State::STARTING, old_state, new_state);
  MaybeSendStateChange(State::STARTED, old_state, new_state);
  MaybeSendStateChange(State::RUNNING, old_state, new_state);
}

void FakeAssistantManagerServiceImpl::MaybeSendStateChange(State state,
                                                           State old_state,
                                                           State target_state) {
  if (state > old_state && state <= target_state) {
    for (auto& observer : state_observers_)
      observer.OnStateChanged(state);
  }
}

}  // namespace assistant
}  // namespace chromeos
