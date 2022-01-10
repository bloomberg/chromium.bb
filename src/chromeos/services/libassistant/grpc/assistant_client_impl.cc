// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/assistant_client_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/assistant/internal/grpc_transport/request_utils.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/alarm_timer_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/audio_utils_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/bootup_settings_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/config_settings_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/display_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/experiment_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/query_interface.pb.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/libassistant/callback_utils.h"
#include "chromeos/services/libassistant/grpc/assistant_client_v1.h"
#include "chromeos/services/libassistant/grpc/external_services/action_service.h"
#include "chromeos/services/libassistant/grpc/grpc_libassistant_client.h"
#include "chromeos/services/libassistant/grpc/services_status_observer.h"
#include "chromeos/services/libassistant/grpc/utils/timer_utils.h"

namespace chromeos {
namespace libassistant {

namespace {

using ::assistant::api::EnableListeningRequest;
using ::assistant::api::EnableListeningResponse;
using ::assistant::api::SetLocaleOverrideRequest;
using ::assistant::api::SetLocaleOverrideResponse;

// Rpc call config constants.
constexpr int kMaxRpcRetries = 5;
constexpr int kDefaultTimeoutMs = 5000;

// Interaction related calls has longer timeout.
constexpr int kAssistantInteractionDefaultTimeoutMs = 20000;

const chromeos::libassistant::StateConfig kDefaultStateConfig =
    chromeos::libassistant::StateConfig{kMaxRpcRetries, kDefaultTimeoutMs};

const chromeos::libassistant::StateConfig kInteractionDefaultStateConfig =
    chromeos::libassistant::StateConfig{kMaxRpcRetries,
                                        kAssistantInteractionDefaultTimeoutMs};

#define LOG_GRPC_STATUS(level, status, func_name)                \
  if (status.ok()) {                                             \
    DVLOG(level) << func_name << " succeed with ok status.";     \
  } else {                                                       \
    LOG(ERROR) << func_name << " failed with a non-ok status.";  \
    LOG(ERROR) << "Error code: " << status.error_code()          \
               << ", error message: " << status.error_message(); \
  }

// Creates a callback for logging the request status. The callback will
// ignore the returned response as it either doesn't contain any information
// we need or is empty.
template <typename Response>
base::OnceCallback<void(const grpc::Status& status, const Response&)>
GetLoggingCallback(const std::string& request_name) {
  return base::BindOnce(
      [](const std::string& request_name, const grpc::Status& status,
         const Response& ignored) { LOG_GRPC_STATUS(2, status, request_name); },
      request_name);
}

}  // namespace

AssistantClientImpl::AssistantClientImpl(
    std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal,
    const std::string& libassistant_service_address,
    const std::string& assistant_service_address)
    : AssistantClientV1(std::move(assistant_manager),
                        assistant_manager_internal),
      grpc_services_(libassistant_service_address, assistant_service_address),
      libassistant_client_(grpc_services_.GrpcLibassistantClient()) {}

AssistantClientImpl::~AssistantClientImpl() = default;

void AssistantClientImpl::StartServices(
    ServicesStatusObserver* services_status_observer) {
  grpc_services_.GetServicesStatusProvider().AddObserver(
      services_status_observer);

  StartGrpcServices();

  AssistantClientV1::StartServices(services_status_observer);
}

bool AssistantClientImpl::StartGrpcServices() {
  return grpc_services_.Start();
}

void AssistantClientImpl::AddExperimentIds(
    const std::vector<std::string>& exp_ids) {
  ::assistant::api::UpdateExperimentIdsRequest request;
  request.set_operation(
      ::assistant::api::UpdateExperimentIdsRequest_Operation_MERGE);
  *request.mutable_experiment_ids() = {exp_ids.begin(), exp_ids.end()};

  libassistant_client_.CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::UpdateExperimentIdsResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::AddSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) {
  NOTIMPLEMENTED();
}

void AssistantClientImpl::RemoveSpeakerIdEnrollmentEventObserver(
    GrpcServicesObserver<OnSpeakerIdEnrollmentEventRequest>* observer) {
  NOTIMPLEMENTED();
}

void AssistantClientImpl::StartSpeakerIdEnrollment(
    const StartSpeakerIdEnrollmentRequest& request) {
  NOTIMPLEMENTED();
}

void AssistantClientImpl::CancelSpeakerIdEnrollment(
    const CancelSpeakerIdEnrollmentRequest& request) {
  NOTIMPLEMENTED();
}

void AssistantClientImpl::GetSpeakerIdEnrollmentInfo(
    const ::assistant::api::GetSpeakerIdEnrollmentInfoRequest& request,
    base::OnceCallback<void(bool user_model_exists)> on_done) {
  NOTIMPLEMENTED();
  std::move(on_done).Run(/*user_model_exists=*/false);
}

void AssistantClientImpl::ResetAllDataAndShutdown() {
  // ResetAllDataAndShutdown request may have high latency. Server
  // recommendation is to set proper deadlines for every RPC.
  constexpr int kResetAllDataAndShutdownTimeoutMs = 10000;
  StateConfig custom_config(kMaxRpcRetries, kResetAllDataAndShutdownTimeoutMs);
  libassistant_client_.CallServiceMethod(
      ::assistant::api::ResetAllDataAndShutdownRequest(),
      GetLoggingCallback<::assistant::api::ResetAllDataAndShutdownResponse>(
          /*request_name=*/__func__),
      custom_config);
}

void AssistantClientImpl::SendDisplayRequest(
    const OnDisplayRequestRequest& request) {
  libassistant_client_.CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::OnDisplayRequestResponse>(
          /*request_name=*/__func__),
      kInteractionDefaultStateConfig);
}

void AssistantClientImpl::AddDisplayEventObserver(
    GrpcServicesObserver<OnAssistantDisplayEventRequest>* observer) {
  grpc_services_.AddAssistantDisplayEventObserver(observer);
}

void AssistantClientImpl::AddDeviceStateEventObserver(
    GrpcServicesObserver<OnDeviceStateEventRequest>* observer) {
  grpc_services_.AddDeviceStateEventObserver(observer);
}

void AssistantClientImpl::SendVoicelessInteraction(
    const ::assistant::api::Interaction& interaction,
    const std::string& description,
    const ::assistant::api::VoicelessOptions& options,
    base::OnceCallback<void(bool)> on_done) {
  ::assistant::api::SendQueryRequest request;
  PopulateSendQueryRequest(interaction, description, options, &request);

  libassistant_client_.CallServiceMethod(
      request,
      base::BindOnce(
          [](base::OnceCallback<void(bool)> on_done, const grpc::Status& status,
             const ::assistant::api::SendQueryResponse& response) {
            std::move(on_done).Run(response.success());
          },
          std::move(on_done)),
      kInteractionDefaultStateConfig);
}

void AssistantClientImpl::RegisterActionModule(
    assistant_client::ActionModule* action_module) {
  grpc_services_.GetActionService()->RegisterActionModule(action_module);
}

void AssistantClientImpl::SendScreenContextRequest(
    const std::vector<std::string>& context_protos) {
  NOTIMPLEMENTED();
}

void AssistantClientImpl::StartVoiceInteraction() {
  libassistant_client_.CallServiceMethod(
      ::assistant::api::StartVoiceQueryRequest(),
      GetLoggingCallback<::assistant::api::StartVoiceQueryResponse>(
          /*request_name=*/__func__),
      kInteractionDefaultStateConfig);
}

void AssistantClientImpl::StopAssistantInteraction(bool cancel_conversation) {
  ::assistant::api::StopQueryRequest request;
  request.set_type(::assistant::api::StopQueryRequest::ACTIVE_INTERNAL);
  request.set_cancel_conversation(cancel_conversation);

  libassistant_client_.CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::StopQueryResponse>(
          /*request_name=*/__func__),
      kInteractionDefaultStateConfig);
}

void AssistantClientImpl::SetAuthenticationInfo(const AuthTokens& tokens) {
  ::assistant::api::SetAuthInfoRequest request;
  // Each token exists of a [gaia_id, auth_token] tuple.
  for (const auto& token : tokens) {
    auto* proto = request.add_tokens();
    proto->set_user_id(token.first);
    proto->set_auth_token(token.second);
  }

  libassistant_client_.CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::SetAuthInfoResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::SetInternalOptions(const std::string& locale,
                                             bool spoken_feedback_enabled) {
  auto internal_options = chromeos::assistant::CreateInternalOptionsProto(
      locale, spoken_feedback_enabled);

  ::assistant::api::SetInternalOptionsRequest request;
  *request.mutable_internal_options() = std::move(internal_options);

  // SetInternalOptions request causes AssistantManager reconfiguration.
  constexpr int kAssistantReconfigureInternalDefaultTimeoutMs = 20000;
  StateConfig custom_config(kMaxRpcRetries,
                            kAssistantReconfigureInternalDefaultTimeoutMs);
  libassistant_client_.CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::SetInternalOptionsResponse>(
          /*request_name=*/__func__),
      custom_config);
}

void AssistantClientImpl::UpdateAssistantSettings(
    const ::assistant::ui::SettingsUiUpdate& update,
    const std::string& user_id,
    base::OnceCallback<void(
        const ::assistant::api::UpdateAssistantSettingsResponse&)> on_done) {
  using ::assistant::api::UpdateAssistantSettingsRequest;
  using ::assistant::api::UpdateAssistantSettingsResponse;

  UpdateAssistantSettingsRequest request;
  // Sets obfuscated gaia id.
  request.set_user_id(user_id);
  // Sets the update to be applied to the settings.
  *request.mutable_update_settings_ui_request()->mutable_settings_update() =
      update;

  auto cb = base::BindOnce(
      [](base::OnceCallback<void(const UpdateAssistantSettingsResponse&)>
             on_done,
         const grpc::Status& status,
         const UpdateAssistantSettingsResponse& response) {
        LOG_GRPC_STATUS(/*level=*/2, status, "UpdateAssistantSettings")
        std::move(on_done).Run(response);
      },
      std::move(on_done));
  libassistant_client_.CallServiceMethod(request, std::move(cb),
                                         kDefaultStateConfig);
}

void AssistantClientImpl::GetAssistantSettings(
    const ::assistant::ui::SettingsUiSelector& selector,
    const std::string& user_id,
    base::OnceCallback<
        void(const ::assistant::api::GetAssistantSettingsResponse&)> on_done) {
  using ::assistant::api::GetAssistantSettingsRequest;
  using ::assistant::api::GetAssistantSettingsResponse;

  GetAssistantSettingsRequest request;
  *request.mutable_get_settings_ui_request()->mutable_selector() = selector;
  request.set_user_id(user_id);

  auto cb = base::BindOnce(
      [](base::OnceCallback<void(const GetAssistantSettingsResponse&)> on_done,
         const grpc::Status& status,
         const GetAssistantSettingsResponse& response) {
        LOG_GRPC_STATUS(/*level=*/2, status, "GetAssistantSettings")
        std::move(on_done).Run(response);
      },
      std::move(on_done));

  libassistant_client_.CallServiceMethod(request, std::move(cb),
                                         kDefaultStateConfig);
}

void AssistantClientImpl::SetLocaleOverride(const std::string& locale) {
  SetLocaleOverrideRequest request;
  request.set_locale(locale);

  libassistant_client_.CallServiceMethod(
      request, GetLoggingCallback<SetLocaleOverrideResponse>(__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::EnableListening(bool listening_enabled) {
  EnableListeningRequest request;
  request.set_enable(listening_enabled);

  libassistant_client_.CallServiceMethod(
      request, GetLoggingCallback<EnableListeningResponse>(__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::AddTimeToTimer(const std::string& id,
                                         const base::TimeDelta& duration) {
  ::assistant::api::AddTimeToTimerRequest request;
  request.set_timer_id(id);
  request.set_extra_time_seconds(duration.InSeconds());
  libassistant_client_.CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::AddTimeToTimerResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::PauseTimer(const std::string& timer_id) {
  ::assistant::api::PauseTimerRequest request;
  request.set_timer_id(timer_id);
  libassistant_client_.CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::PauseTimerResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::RemoveTimer(const std::string& timer_id) {
  ::assistant::api::RemoveTimerRequest request;
  request.set_timer_id(timer_id);
  libassistant_client_.CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::RemoveTimerResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::ResumeTimer(const std::string& timer_id) {
  ::assistant::api::ResumeTimerRequest request;
  request.set_timer_id(timer_id);
  libassistant_client_.CallServiceMethod(
      request,
      GetLoggingCallback<::assistant::api::ResumeTimerResponse>(
          /*request_name=*/__func__),
      kDefaultStateConfig);
}

void AssistantClientImpl::GetTimers(
    base::OnceCallback<void(const std::vector<assistant::AssistantTimer>&)>
        on_done) {
  ::assistant::api::GetTimersResponse response;

  libassistant_client_.CallServiceMethod(
      ::assistant::api::GetTimersRequest(),
      base::BindOnce(
          [](base::OnceCallback<void(
                 const std::vector<assistant::AssistantTimer>&)> on_done,
             const grpc::Status& status,
             const ::assistant::api::GetTimersResponse& response) {
            if (status.ok()) {
              std::move(on_done).Run(
                  ConstructAssistantTimersFromProto(response.timers()));
            } else {
              std::move(on_done).Run(/*timers=*/{});
            }
          },
          std::move(on_done)),
      kDefaultStateConfig);
}

void AssistantClientImpl::AddAlarmTimerEventObserver(
    GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>*
        observer) {
  grpc_services_.AddAlarmTimerEventObserver(observer);
}

// static
std::unique_ptr<AssistantClient> AssistantClient::Create(
    std::unique_ptr<assistant_client::AssistantManager> assistant_manager,
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  if (chromeos::assistant::features::IsLibAssistantV2Enabled()) {
    // Note that we should *not* depend on |assistant_manager_internal| for V2,
    // so |assistant_manager_internal| will be nullptr after the migration has
    // done.
    return std::make_unique<AssistantClientImpl>(
        std::move(assistant_manager), assistant_manager_internal,
        assistant::kLibassistantServiceAddress,
        assistant::kAssistantServiceAddress);
  }

  return std::make_unique<AssistantClientV1>(std::move(assistant_manager),
                                             assistant_manager_internal);
}

}  // namespace libassistant
}  // namespace chromeos
