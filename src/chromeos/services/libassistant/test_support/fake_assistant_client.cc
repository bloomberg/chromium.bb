// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/test_support/fake_assistant_client.h"

#include "base/callback.h"
#include "base/test/bind.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "chromeos/assistant/internal/test_support/fake_alarm_timer_manager.h"
#include "chromeos/services/libassistant/grpc/utils/timer_utils.h"
#include "chromeos/services/libassistant/public/cpp/assistant_timer.h"

namespace chromeos {
namespace libassistant {

FakeAssistantClient::FakeAssistantClient(
    std::unique_ptr<assistant::FakeAssistantManager> assistant_manager,
    assistant::FakeAssistantManagerInternal* assistant_manager_internal)
    : AssistantClient(std::move(assistant_manager),
                      assistant_manager_internal) {}

FakeAssistantClient::~FakeAssistantClient() = default;

void FakeAssistantClient::StartServices(
    ServicesStatusObserver* services_status_observer) {}

void FakeAssistantClient::SetChromeOSApiDelegate(
    assistant_client::ChromeOSApiDelegate* delegate) {}

bool FakeAssistantClient::StartGrpcServices() {
  return true;
}

void FakeAssistantClient::AddExperimentIds(
    const std::vector<std::string>& exp_ids) {}

void FakeAssistantClient::AddSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) {}

void FakeAssistantClient::RemoveSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) {}

void FakeAssistantClient::StartSpeakerIdEnrollment(
    const StartSpeakerIdEnrollmentRequest& request) {}

void FakeAssistantClient::CancelSpeakerIdEnrollment(
    const CancelSpeakerIdEnrollmentRequest& request) {}

void FakeAssistantClient::GetSpeakerIdEnrollmentInfo(
    const ::assistant::api::GetSpeakerIdEnrollmentInfoRequest& request,
    base::OnceCallback<void(bool user_model_exists)> on_done) {}

void FakeAssistantClient::ResetAllDataAndShutdown() {}

void FakeAssistantClient::SendDisplayRequest(
    const OnDisplayRequestRequest& request) {}

void FakeAssistantClient::AddDisplayEventObserver(
    GrpcServicesObserver<OnAssistantDisplayEventRequest>* observer) {}

void FakeAssistantClient::ResumeCurrentStream() {}

void FakeAssistantClient::PauseCurrentStream() {}

void FakeAssistantClient::SetExternalPlaybackState(
    const MediaStatus& status_proto) {}

void FakeAssistantClient::AddDeviceStateEventObserver(
    GrpcServicesObserver<OnDeviceStateEventRequest>* observer) {}

void FakeAssistantClient::SendVoicelessInteraction(
    const ::assistant::api::Interaction& interaction,
    const std::string& description,
    const ::assistant::api::VoicelessOptions& options,
    base::OnceCallback<void(bool)> on_done) {}

void FakeAssistantClient::RegisterActionModule(
    assistant_client::ActionModule* action_module) {}

void FakeAssistantClient::SendScreenContextRequest(
    const std::vector<std::string>& context_protos) {}

void FakeAssistantClient::StartVoiceInteraction() {}

void FakeAssistantClient::StopAssistantInteraction(bool cancel_conversation) {}

void FakeAssistantClient::SetInternalOptions(const std::string& locale,
                                             bool spoken_feedback_enabled) {}

void FakeAssistantClient::SetAuthenticationInfo(const AuthTokens& tokens) {}

void FakeAssistantClient::UpdateAssistantSettings(
    const ::assistant::ui::SettingsUiUpdate& settings,
    const std::string& user_id,
    base::OnceCallback<void(
        const ::assistant::api::UpdateAssistantSettingsResponse&)> on_done) {}

void FakeAssistantClient::GetAssistantSettings(
    const ::assistant::ui::SettingsUiSelector& selector,
    const std::string& user_id,
    base::OnceCallback<
        void(const ::assistant::api::GetAssistantSettingsResponse&)> on_done) {}

void FakeAssistantClient::SetLocaleOverride(const std::string& locale) {}

std::string FakeAssistantClient::GetDeviceId() {
  return assistant_manager()->GetDeviceId();
}

void FakeAssistantClient::SetDeviceAttributes(bool enable_dark_mode) {}

void FakeAssistantClient::EnableListening(bool listening_enabled) {}

void FakeAssistantClient::AddTimeToTimer(const std::string& id,
                                         const base::TimeDelta& duration) {
  fake_alarm_timer_manager()->AddTimeToTimer(id, duration.InSeconds());
}

void FakeAssistantClient::PauseTimer(const std::string& timer_id) {
  fake_alarm_timer_manager()->PauseTimer(timer_id);
}

void FakeAssistantClient::RemoveTimer(const std::string& timer_id) {
  fake_alarm_timer_manager()->RemoveEvent(timer_id);
}

void FakeAssistantClient::ResumeTimer(const std::string& timer_id) {
  fake_alarm_timer_manager()->ResumeTimer(timer_id);
}

void FakeAssistantClient::GetTimers(
    base::OnceCallback<void(const std::vector<assistant::AssistantTimer>&)>
        on_done) {
  return std::move(on_done).Run(GetAllCurrentTimersFromEvents(
      fake_alarm_timer_manager()->GetAllEvents()));
}

void FakeAssistantClient::AddAlarmTimerEventObserver(
    GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>*
        observer) {
  timer_observer_ = observer;

  fake_alarm_timer_manager()->RegisterRingingStateListener(
      [this]() { GetAndNotifyTimerStatus(); });

  fake_alarm_timer_manager()->RegisterAlarmActionListener(
      [this](assistant_client::AlarmTimerManager::EventActionType ignore) {
        GetAndNotifyTimerStatus();
      });
}

assistant::FakeAlarmTimerManager*
FakeAssistantClient::fake_alarm_timer_manager() {
  return reinterpret_cast<assistant::FakeAlarmTimerManager*>(
      assistant_manager_internal()->GetAlarmTimerManager());
}

void FakeAssistantClient::GetAndNotifyTimerStatus() {
  GetTimers(base::BindLambdaForTesting(
      [this](const std::vector<assistant::AssistantTimer>& timers) {
        timer_observer_->OnGrpcMessage(
            CreateOnAlarmTimerEventRequestProtoForV1(timers));
      }));
}

}  // namespace libassistant
}  // namespace chromeos
