// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/grpc/external_services/event_handler_driver.h"

#include "chromeos/assistant/internal/libassistant_util.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/event_notification_interface.pb.h"

namespace chromeos {
namespace libassistant {

namespace {

constexpr char kAlarmTimerEventName[] = "AlarmTimerEvent";
constexpr char kAssistantDisplayEventName[] = "AssistantDisplayEvent";
constexpr char kDeviceStateEventName[] = "DeviceStateEvent";
constexpr char kHandlerMethodName[] = "OnEventFromLibas";

template <typename EventSelection>
void PopulateRequest(const std::string& assistant_service_address,
                     const std::string& event_name,
                     ::assistant::api::RegisterEventHandlerRequest* request,
                     EventSelection* event_selection) {
  event_selection->set_select_all(true);
  auto* external_handler = request->mutable_handler();
  external_handler->set_server_address(assistant_service_address);
  external_handler->set_service_name(
      chromeos::assistant::GetLibassistGrpcServiceName(event_name));
  external_handler->set_handler_method(kHandlerMethodName);
}

}  // namespace

template <>
::assistant::api::RegisterEventHandlerRequest
CreateRegistrationRequest<::assistant::api::AlarmTimerEventHandlerInterface>(
    const std::string& assistant_service_address) {
  ::assistant::api::RegisterEventHandlerRequest request;
  PopulateRequest(assistant_service_address, kAlarmTimerEventName, &request,
                  request.mutable_alarm_timer_events_to_handle());
  return request;
}

template <>
::assistant::api::RegisterEventHandlerRequest CreateRegistrationRequest<
    ::assistant::api::AssistantDisplayEventHandlerInterface>(
    const std::string& assistant_service_address) {
  ::assistant::api::RegisterEventHandlerRequest request;
  PopulateRequest(assistant_service_address, kAssistantDisplayEventName,
                  &request,
                  request.mutable_assistant_display_events_to_handle());
  return request;
}

template <>
::assistant::api::RegisterEventHandlerRequest
CreateRegistrationRequest<::assistant::api::DeviceStateEventHandlerInterface>(
    const std::string& assistant_service_address) {
  ::assistant::api::RegisterEventHandlerRequest request;
  PopulateRequest(assistant_service_address, kDeviceStateEventName, &request,
                  request.mutable_device_state_events_to_handle());
  return request;
}

}  // namespace libassistant
}  // namespace chromeos
