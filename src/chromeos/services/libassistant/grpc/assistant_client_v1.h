// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_V1_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_V1_H_

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/services/libassistant/grpc/assistant_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace assistant_client {
class AlarmTimerManager;
}  // namespace assistant_client

namespace chromeos {
namespace libassistant {

class ServicesStatusObserver;

class AssistantClientV1 : public AssistantClient {
 public:
  AssistantClientV1(
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal);
  ~AssistantClientV1() override;

  // chromeos::libassistant::AssistantClient:
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
  void SetAuthenticationInfo(const AuthTokens& tokens) override;
  void SetInternalOptions(const std::string& locale,
                          bool spoken_feedback_enabled) override;
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
  class DeviceStateListener;
  class DisplayConnectionImpl;
  class MediaManagerListener;

  void AddMediaManagerListener();

  void NotifyAllServicesReady();

  void NotifyDeviceStateEvent(const OnDeviceStateEventRequest& request);

  void OnSpeakerIdEnrollmentUpdate(
      const assistant_client::SpeakerIdEnrollmentUpdate& update);

  assistant_client::AlarmTimerManager* alarm_timer_manager();

  // Get the timer status and notify the `timer_observer_`.
  void GetAndNotifyTimerStatus();

  absl::optional<bool> dark_mode_enabled_;

  std::unique_ptr<DeviceStateListener> device_state_listener_;

  std::unique_ptr<DisplayConnectionImpl> display_connection_;
  std::unique_ptr<MediaManagerListener> media_manager_listener_;

  base::ObserverList<GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>>
      speaker_event_observer_list_;

  base::ObserverList<GrpcServicesObserver<OnDeviceStateEventRequest>>
      device_state_event_observer_list_;

  base::ObserverList<
      GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>>
      timer_event_observer_list_;

  ServicesStatusObserver* services_status_observer_ = nullptr;

  base::WeakPtrFactory<AssistantClientV1> weak_factory_{this};
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_V1_H_
