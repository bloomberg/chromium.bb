// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/external_services/grpc_services_initializer.h"

#include <memory>

#include "base/time/time.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_service.grpc.pb.h"
#include "chromeos/services/libassistant/grpc/external_services/action_service.h"
#include "chromeos/services/libassistant/grpc/external_services/customer_registration_client.h"
#include "chromeos/services/libassistant/grpc/external_services/heartbeat_event_handler_driver.h"
#include "chromeos/services/libassistant/grpc/grpc_libassistant_client.h"
#include "chromeos/services/libassistant/grpc/grpc_util.h"
#include "third_party/grpc/src/include/grpc/grpc_security_constants.h"
#include "third_party/grpc/src/include/grpc/impl/codegen/grpc_types.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"
#include "third_party/grpc/src/include/grpcpp/security/credentials.h"
#include "third_party/grpc/src/include/grpcpp/security/server_credentials.h"
#include "third_party/grpc/src/include/grpcpp/support/channel_arguments.h"

namespace chromeos {
namespace libassistant {

namespace {

// Desired time between consecutive heartbeats.
constexpr base::TimeDelta kHeartbeatInterval = base::Seconds(2);

}  // namespace

GrpcServicesInitializer::GrpcServicesInitializer(
    const std::string& libassistant_service_address,
    const std::string& assistant_service_address)
    : ServicesInitializerBase(
          /*cq_thread_name=*/assistant_service_address + ".GrpcCQ",
          /*main_task_runner=*/base::SequencedTaskRunnerHandle::Get()),
      assistant_service_address_(assistant_service_address),
      libassistant_service_address_(libassistant_service_address) {
  DCHECK(!libassistant_service_address.empty());
  DCHECK(!assistant_service_address.empty());

  InitLibassistGrpcClient();
  InitAssistantGrpcServer();

  customer_registration_client_ = std::make_unique<CustomerRegistrationClient>(
      assistant_service_address_, kHeartbeatInterval,
      libassistant_client_.get());
}

GrpcServicesInitializer::~GrpcServicesInitializer() {
  if (assistant_grpc_server_)
    assistant_grpc_server_->Shutdown();

  StopCQ();
}

bool GrpcServicesInitializer::Start() {
  // Starts the server after all drivers have been initiated.
  assistant_grpc_server_ = server_builder_.BuildAndStart();

  if (!assistant_grpc_server_) {
    LOG(ERROR) << "Failed to start a server for ChromeOS Assistant gRPC.";
    return false;
  }

  DVLOG(1) << "Started ChromeOS Assistant gRPC service";

  RegisterEventHandlers();
  StartCQ();
  customer_registration_client_->Start();
  return true;
}

void GrpcServicesInitializer::AddAlarmTimerEventObserver(
    GrpcServicesObserver<::assistant::api::OnAlarmTimerEventRequest>*
        observer) {
  alarm_timer_event_handler_driver_->AddObserver(observer);
}

void GrpcServicesInitializer::AddAssistantDisplayEventObserver(
    GrpcServicesObserver<::assistant::api::OnAssistantDisplayEventRequest>*
        observer) {
  assistant_display_event_handler_driver_->AddObserver(observer);
}

void GrpcServicesInitializer::AddDeviceStateEventObserver(
    GrpcServicesObserver<::assistant::api::OnDeviceStateEventRequest>*
        observer) {
  device_state_event_handler_driver_->AddObserver(observer);
}

ActionService* GrpcServicesInitializer::GetActionService() {
  return action_handler_driver_.get();
}

GrpcLibassistantClient& GrpcServicesInitializer::GrpcLibassistantClient() {
  return *libassistant_client_;
}

void GrpcServicesInitializer::InitDrivers(grpc::ServerBuilder* server_builder) {
  // Inits heartbeat driver.
  heartbeat_driver_ =
      std::make_unique<HeartbeatEventHandlerDriver>(&server_builder_);
  heartbeat_event_observation_.Observe(heartbeat_driver_.get());
  service_drivers_.emplace_back(heartbeat_driver_.get());

  // Inits action service.
  action_handler_driver_ = std::make_unique<ActionService>(
      &server_builder_, libassistant_client_.get(), assistant_service_address_);
  service_drivers_.emplace_back(action_handler_driver_.get());

  // Inits other event handler drivers.
  alarm_timer_event_handler_driver_ = std::make_unique<
      EventHandlerDriver<::assistant::api::AlarmTimerEventHandlerInterface>>(
      &server_builder_, libassistant_client_.get(), assistant_service_address_);
  service_drivers_.emplace_back(alarm_timer_event_handler_driver_.get());

  assistant_display_event_handler_driver_ = std::make_unique<EventHandlerDriver<
      ::assistant::api::AssistantDisplayEventHandlerInterface>>(
      &server_builder_, libassistant_client_.get(), assistant_service_address_);
  service_drivers_.emplace_back(assistant_display_event_handler_driver_.get());

  device_state_event_handler_driver_ = std::make_unique<
      EventHandlerDriver<::assistant::api::DeviceStateEventHandlerInterface>>(
      &server_builder_, libassistant_client_.get(), assistant_service_address_);
  service_drivers_.emplace_back(device_state_event_handler_driver_.get());
}

void GrpcServicesInitializer::InitLibassistGrpcClient() {
  grpc::ChannelArguments channel_args;
  channel_args.SetInt(GRPC_ARG_INITIAL_RECONNECT_BACKOFF_MS, 200);
  channel_args.SetInt(GRPC_ARG_MIN_RECONNECT_BACKOFF_MS, 200);
  channel_args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 2000);
  grpc_local_connect_type connect_type =
      GetGrpcLocalConnectType(libassistant_service_address_);

  auto channel = grpc::CreateCustomChannel(
      libassistant_service_address_,
      ::grpc::experimental::LocalCredentials(connect_type), channel_args);

  libassistant_client_ =
      std::make_unique<chromeos::libassistant::GrpcLibassistantClient>(channel);
}

void GrpcServicesInitializer::InitAssistantGrpcServer() {
  auto connect_type = GetGrpcLocalConnectType(assistant_service_address_);
  // Listen on the given address with the specified credentials.
  server_builder_.AddListeningPort(
      assistant_service_address_,
      ::grpc::experimental::LocalServerCredentials(connect_type));
  RegisterServicesAndInitCQ(&server_builder_);
}

void GrpcServicesInitializer::RegisterEventHandlers() {
  alarm_timer_event_handler_driver_->StartRegistration();
  assistant_display_event_handler_driver_->StartRegistration();
  device_state_event_handler_driver_->StartRegistration();
}

}  // namespace libassistant
}  // namespace chromeos
