// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_H_

#include <memory>

#include "base/callback_forward.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/device_state_event.pb.h"
#include "chromeos/services/libassistant/grpc/external_services/grpc_services_observer.h"
#include "chromeos/services/libassistant/grpc/services_status_observer.h"
#include "chromeos/services/libassistant/public/cpp/assistant_timer.h"

namespace assistant {
namespace api {
class CancelSpeakerIdEnrollmentRequest;
class GetSpeakerIdEnrollmentInfoRequest;
class GetAssistantSettingsResponse;
class Interaction;
class OnAlarmTimerEventRequest;
class OnAssistantDisplayEventRequest;
class OnDeviceStateEventRequest;
class OnDisplayRequestRequest;
class OnSpeakerIdEnrollmentEventRequest;
class StartSpeakerIdEnrollmentRequest;
class UpdateAssistantSettingsResponse;
class VoicelessOptions;

namespace events {
class SpeakerIdEnrollmentEvent;
}  // namespace events
}  // namespace api
}  // namespace assistant

namespace assistant {
namespace ui {
class SettingsUiSelector;
class SettingsUiUpdate;
}  // namespace ui
}  // namespace assistant

namespace assistant_client {
class ActionModule;
class AssistantManager;
class AssistantManagerInternal;
class ChromeOSApiDelegate;
}  // namespace assistant_client

namespace chromeos {
namespace libassistant {

// The Libassistant customer class which establishes a gRPC connection to
// Libassistant and provides an interface for interacting with gRPC Libassistant
// client. It helps to build request/response proto messages internally for each
// specific method below and call the appropriate gRPC (IPC) client method.
class AssistantClient {
 public:
  // Speaker Id Enrollment:
  using CancelSpeakerIdEnrollmentRequest =
      ::assistant::api::CancelSpeakerIdEnrollmentRequest;
  using GetSpeakerIdEnrollmentInfoRequest =
      ::assistant::api::GetSpeakerIdEnrollmentInfoRequest;
  using StartSpeakerIdEnrollmentRequest =
      ::assistant::api::StartSpeakerIdEnrollmentRequest;
  using SpeakerIdEnrollmentEvent =
      ::assistant::api::events::SpeakerIdEnrollmentEvent;
  using OnSpeakerIdEnrollmentEventRequest =
      ::assistant::api::OnSpeakerIdEnrollmentEventRequest;

  // Display:
  using OnAssistantDisplayEventRequest =
      ::assistant::api::OnAssistantDisplayEventRequest;
  using OnDisplayRequestRequest = ::assistant::api::OnDisplayRequestRequest;

  // Media:
  using MediaStatus = ::assistant::api::events::DeviceState::MediaStatus;
  using OnDeviceStateEventRequest = ::assistant::api::OnDeviceStateEventRequest;

  // Each authentication token exists of a [gaia_id, access_token] tuple.
  using AuthTokens = std::vector<std::pair<std::string, std::string>>;

  AssistantClient(
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal);
  AssistantClient(const AssistantClient&) = delete;
  AssistantClient& operator=(const AssistantClient&) = delete;
  virtual ~AssistantClient();

  // Starts Libassistant services. |services_status_observer| will get notified
  // on new status change.
  virtual void StartServices(
      ServicesStatusObserver* services_status_observer) = 0;

  virtual void SetChromeOSApiDelegate(
      assistant_client::ChromeOSApiDelegate* delegate) = 0;

  // 1. Start a gRPC server which hosts the services that Libassistant depends
  // on (maybe called by Libassistant) or receive events from Libassistant.
  // 2. Register this client as a customer of Libassistant by sending
  // RegisterCustomerRequest to Libassistant periodically. All supported
  // services should be registered first before calling this method.
  virtual bool StartGrpcServices() = 0;

  virtual void AddExperimentIds(const std::vector<std::string>& exp_ids) = 0;


  // Speaker Id Enrollment methods.
  virtual void AddSpeakerIdEnrollmentEventObserver(
      GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) = 0;
  virtual void RemoveSpeakerIdEnrollmentEventObserver(
      GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) = 0;
  virtual void StartSpeakerIdEnrollment(
      const StartSpeakerIdEnrollmentRequest& request) = 0;
  virtual void CancelSpeakerIdEnrollment(
      const CancelSpeakerIdEnrollmentRequest& request) = 0;
  virtual void GetSpeakerIdEnrollmentInfo(
      const GetSpeakerIdEnrollmentInfoRequest& request,
      base::OnceCallback<void(bool user_model_exists)> on_done) = 0;

  virtual void ResetAllDataAndShutdown() = 0;

  // Display methods.
  virtual void SendDisplayRequest(const OnDisplayRequestRequest& request) = 0;
  virtual void AddDisplayEventObserver(
      GrpcServicesObserver<OnAssistantDisplayEventRequest>* observer) = 0;

  // Media methods.
  virtual void ResumeCurrentStream() = 0;
  virtual void PauseCurrentStream() = 0;
  // Sets the current media status of media playing outside of libassistant.
  // Setting external state will stop any internally playing media.
  virtual void SetExternalPlaybackState(const MediaStatus& status_proto) = 0;

  virtual void AddDeviceStateEventObserver(
      GrpcServicesObserver<OnDeviceStateEventRequest>* observer) = 0;

  // Conversation methods.
  virtual void SendVoicelessInteraction(
      const ::assistant::api::Interaction& interaction,
      const std::string& description,
      const ::assistant::api::VoicelessOptions& options,
      base::OnceCallback<void(bool)> on_done) = 0;
  virtual void RegisterActionModule(
      assistant_client::ActionModule* action_module) = 0;
  virtual void SendScreenContextRequest(
      const std::vector<std::string>& context_protos) = 0;
  virtual void StartVoiceInteraction() = 0;
  virtual void StopAssistantInteraction(bool cancel_conversation) = 0;

  // Settings-related functionality during bootup:
  virtual void SetAuthenticationInfo(const AuthTokens& tokens) = 0;
  virtual void SetInternalOptions(const std::string& locale,
                                  bool spoken_feedback_enabled) = 0;

  // Settings-related functionality after fully started:
  virtual void UpdateAssistantSettings(
      const ::assistant::ui::SettingsUiUpdate& settings,
      const std::string& user_id,
      base::OnceCallback<
          void(const ::assistant::api::UpdateAssistantSettingsResponse&)>
          on_done) = 0;
  virtual void GetAssistantSettings(
      const ::assistant::ui::SettingsUiSelector& selector,
      const std::string& user_id,
      base::OnceCallback<void(
          const ::assistant::api::GetAssistantSettingsResponse&)> on_done) = 0;
  virtual void SetLocaleOverride(const std::string& locale) = 0;
  virtual void SetDeviceAttributes(bool enable_dark_mode) = 0;
  virtual std::string GetDeviceId() = 0;

  // Audio-related functionality:
  // Enables or disables audio input pipeline.
  virtual void EnableListening(bool listening_enabled) = 0;

  // Alarm/timer-related functionality:
  // Adds extra time to the timer.
  virtual void AddTimeToTimer(const std::string& id,
                              const base::TimeDelta& duration) = 0;
  // Pauses the specified timer. This will be a no-op if the |timer_id| is
  // invalid.
  virtual void PauseTimer(const std::string& timer_id) = 0;
  // Removes and cancels the timer.
  virtual void RemoveTimer(const std::string& timer_id) = 0;
  // Resumes the specified timer (expected to be in paused state).
  virtual void ResumeTimer(const std::string& timer_id) = 0;
  // Returns the list of all currently scheduled, ringing or paused timers in
  // the callback.
  virtual void GetTimers(
      base::OnceCallback<void(const std::vector<assistant::AssistantTimer>&)>
          on_done) = 0;

  // Registers |observer| to get notified on any alarm/timer status change.
  virtual void AddAlarmTimerEventObserver(
      GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>*
          observer) = 0;

  // Will not return nullptr.
  assistant_client::AssistantManager* assistant_manager() {
    return assistant_manager_.get();
  }
  // Will not return nullptr.
  assistant_client::AssistantManagerInternal* assistant_manager_internal() {
    return assistant_manager_internal_;
  }

  // Creates an instance of AssistantClient, the returned instance could be
  // LibAssistant V1 or V2 based depending on the current flags. It should be
  // transparent to the caller.
  static std::unique_ptr<AssistantClient> Create(
      std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
      assistant_client::AssistantManagerInternal* assistant_manager_internal);

 protected:
  void ResetAssistantManager();

 private:
  std::unique_ptr<assistant_client::AssistantManager> assistant_manager_;
  assistant_client::AssistantManagerInternal* assistant_manager_internal_ =
      nullptr;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_GRPC_ASSISTANT_CLIENT_H_
