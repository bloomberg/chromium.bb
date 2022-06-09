// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_ASSISTANT_CLIENT_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_ASSISTANT_CLIENT_H_

#include "chromeos/assistant/internal/test_support/fake_assistant_manager.h"
#include "chromeos/assistant/internal/test_support/fake_assistant_manager_internal.h"
#include "chromeos/services/libassistant/grpc/assistant_client.h"

namespace chromeos {
namespace libassistant {

class FakeAssistantClient : public AssistantClient {
 public:
  FakeAssistantClient(
      std::unique_ptr<assistant::FakeAssistantManager> assistant_manager,
      assistant::FakeAssistantManagerInternal* assistant_manager_internal);
  ~FakeAssistantClient() override;

  // chromeos::libassistant::AssistantClient:
  assistant::FakeAssistantManager* assistant_manager() {
    return reinterpret_cast<assistant::FakeAssistantManager*>(
        AssistantClient::assistant_manager());
  }
  assistant::FakeAssistantManagerInternal* assistant_manager_internal() {
    return reinterpret_cast<assistant::FakeAssistantManagerInternal*>(
        AssistantClient::assistant_manager_internal());
  }

  void StartServices(ServicesStatusObserver* services_status_observer) override;
  void SetChromeOSApiDelegate(
      assistant_client::ChromeOSApiDelegate* delegate) override;
  bool StartGrpcServices() override;
  void AddExperimentIds(const std::vector<std::string>& exp_ids) override;
  void AddSpeakerIdEnrollmentEventObserver(
      GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer)
      override;
  void RemoveSpeakerIdEnrollmentEventObserver(
      GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer)
      override;
  void StartSpeakerIdEnrollment(
      const StartSpeakerIdEnrollmentRequest& request) override;
  void CancelSpeakerIdEnrollment(
      const CancelSpeakerIdEnrollmentRequest& request) override;
  void GetSpeakerIdEnrollmentInfo(
      const GetSpeakerIdEnrollmentInfoRequest& request,
      base::OnceCallback<void(bool user_model_exists)> on_done) override;
  void ResetAllDataAndShutdown() override;
  void SendDisplayRequest(const OnDisplayRequestRequest& request) override;
  void AddDisplayEventObserver(
      GrpcServicesObserver<OnAssistantDisplayEventRequest>* observer) override;
  void ResumeCurrentStream() override;
  void PauseCurrentStream() override;
  void SetExternalPlaybackState(const MediaStatus& status_proto) override;
  void AddDeviceStateEventObserver(
      GrpcServicesObserver<OnDeviceStateEventRequest>* observer) override;
  void SendVoicelessInteraction(
      const ::assistant::api::Interaction& interaction,
      const std::string& description,
      const ::assistant::api::VoicelessOptions& options,
      base::OnceCallback<void(bool)> on_done) override;
  void RegisterActionModule(
      assistant_client::ActionModule* action_module) override;
  void SendScreenContextRequest(
      const std::vector<std::string>& context_protos) override;
  void StartVoiceInteraction() override;
  void StopAssistantInteraction(bool cancel_conversation) override;
  void SetInternalOptions(const std::string& locale,
                          bool spoken_feedback_enabled) override;
  void SetAuthenticationInfo(const AuthTokens& tokens) override;
  void UpdateAssistantSettings(
      const ::assistant::ui::SettingsUiUpdate& settings,
      const std::string& user_id,
      base::OnceCallback<void(
          const ::assistant::api::UpdateAssistantSettingsResponse&)> on_done)
      override;
  void GetAssistantSettings(
      const ::assistant::ui::SettingsUiSelector& selector,
      const std::string& user_id,
      base::OnceCallback<
          void(const ::assistant::api::GetAssistantSettingsResponse&)> on_done)
      override;
  void SetLocaleOverride(const std::string& locale) override;
  void SetDeviceAttributes(bool enable_dark_mode) override;
  std::string GetDeviceId() override;
  void EnableListening(bool listening_enabled) override;
  void AddTimeToTimer(const std::string& id,
                      const base::TimeDelta& duration) override;
  void PauseTimer(const std::string& timer_id) override;
  void RemoveTimer(const std::string& timer_id) override;
  void ResumeTimer(const std::string& timer_id) override;
  void GetTimers(
      base::OnceCallback<void(const std::vector<assistant::AssistantTimer>&)>
          on_done) override;
  void AddAlarmTimerEventObserver(
      GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>*
          observer) override;

 private:
  assistant::FakeAlarmTimerManager* fake_alarm_timer_manager();
  void GetAndNotifyTimerStatus();

  GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>*
      timer_observer_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_FAKE_ASSISTANT_CLIENT_H_
