// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_channel/cast_message_handler.h"

#include <tuple>

#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "components/cast_channel/cast_socket_service.h"
#include "services/data_decoder/public/cpp/safe_json_parser.h"
#include "services/service_manager/public/cpp/connector.h"

namespace cast_channel {

namespace {

// The max launch timeout amount for session launch requests.
constexpr base::TimeDelta kLaunchMaxTimeout = base::TimeDelta::FromMinutes(2);

// Default timeout amount for requests waiting for a response.
constexpr base::TimeDelta kRequestTimeout = base::TimeDelta::FromSeconds(5);

void ReportParseError(const std::string& error) {
  DVLOG(2) << "Error parsing JSON message: " << error;
}

}  // namespace

GetAppAvailabilityRequest::GetAppAvailabilityRequest(
    int request_id,
    GetAppAvailabilityCallback callback,
    const base::TickClock* clock,
    const std::string& app_id)
    : PendingRequest(request_id, std::move(callback), clock), app_id(app_id) {}

GetAppAvailabilityRequest::~GetAppAvailabilityRequest() = default;

VirtualConnection::VirtualConnection(int channel_id,
                                     const std::string& source_id,
                                     const std::string& destination_id)
    : channel_id(channel_id),
      source_id(source_id),
      destination_id(destination_id) {}
VirtualConnection::~VirtualConnection() = default;

bool VirtualConnection::operator<(const VirtualConnection& other) const {
  return std::tie(channel_id, source_id, destination_id) <
         std::tie(other.channel_id, other.source_id, other.destination_id);
}

InternalMessage::InternalMessage(CastMessageType type, base::Value message)
    : type(type), message(std::move(message)) {}
InternalMessage::~InternalMessage() = default;

CastMessageHandler::CastMessageHandler(
    CastSocketService* socket_service,
    std::unique_ptr<service_manager::Connector> connector,
    const std::string& data_decoder_batch_id,
    const std::string& user_agent,
    const std::string& browser_version,
    const std::string& locale)
    : sender_id_(base::StringPrintf("sender-%d", base::RandInt(0, 1000000))),
      connector_(std::move(connector)),
      data_decoder_batch_id_(data_decoder_batch_id),
      user_agent_(user_agent),
      browser_version_(browser_version),
      locale_(locale),
      socket_service_(socket_service),
      clock_(base::DefaultTickClock::GetInstance()),
      weak_ptr_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  socket_service_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&CastSocketService::AddObserver,
                                base::Unretained(socket_service_),
                                base::Unretained(this)));
}

CastMessageHandler::~CastMessageHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  socket_service_->RemoveObserver(this);
}

void CastMessageHandler::EnsureConnection(int channel_id,
                                          const std::string& source_id,
                                          const std::string& destination_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CastSocket* socket = socket_service_->GetSocket(channel_id);
  if (!socket) {
    DVLOG(2) << __func__ << ": socket not found: " << channel_id;
    return;
  }

  DoEnsureConnection(socket, source_id, destination_id);
}

CastMessageHandler::PendingRequests*
CastMessageHandler::GetOrCreatePendingRequests(int channel_id) {
  CastMessageHandler::PendingRequests* requests = nullptr;
  auto pending_it = pending_requests_.find(channel_id);
  if (pending_it != pending_requests_.end()) {
    return pending_it->second.get();
  }

  auto new_requests = std::make_unique<CastMessageHandler::PendingRequests>();
  requests = new_requests.get();
  pending_requests_.emplace(channel_id, std::move(new_requests));
  return requests;
}

void CastMessageHandler::RequestAppAvailability(
    CastSocket* socket,
    const std::string& app_id,
    GetAppAvailabilityCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int channel_id = socket->id();
  auto* requests = GetOrCreatePendingRequests(channel_id);
  int request_id = NextRequestId();

  DVLOG(2) << __func__ << ", channel_id: " << channel_id
           << ", app_id: " << app_id << ", request_id: " << request_id;
  if (requests->AddAppAvailabilityRequest(
          std::make_unique<GetAppAvailabilityRequest>(
              request_id, std::move(callback), clock_, app_id))) {
    SendCastMessage(socket, CreateGetAppAvailabilityRequest(
                                sender_id_, request_id, app_id));
  }
}

void CastMessageHandler::SendBroadcastMessage(
    int channel_id,
    const std::vector<std::string>& app_ids,
    const BroadcastRequest& request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CastSocket* socket = socket_service_->GetSocket(channel_id);
  if (!socket) {
    DVLOG(2) << __func__ << ": socket not found: " << channel_id;
    return;
  }

  int request_id = NextRequestId();
  DVLOG(2) << __func__ << ", channel_id: " << channel_id
           << ", request_id: " << request_id;

  // Note: Even though the message is formatted like a request, we don't care
  // about the response, as broadcasts are fire-and-forget.
  CastMessage message =
      CreateBroadcastRequest(sender_id_, request_id, app_ids, request);
  SendCastMessage(socket, message);
}

void CastMessageHandler::LaunchSession(int channel_id,
                                       const std::string& app_id,
                                       base::TimeDelta launch_timeout,
                                       LaunchSessionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CastSocket* socket = socket_service_->GetSocket(channel_id);
  if (!socket) {
    DVLOG(2) << __func__ << ": socket not found: " << channel_id;
    std::move(callback).Run(LaunchSessionResponse());
    return;
  }

  auto* requests = GetOrCreatePendingRequests(channel_id);
  int request_id = NextRequestId();
  // Cap the max launch timeout to avoid long-living pending requests.
  launch_timeout = std::min(launch_timeout, kLaunchMaxTimeout);
  DVLOG(2) << __func__ << ", channel_id: " << channel_id
           << ", request_id: " << request_id;
  if (requests->AddLaunchRequest(std::make_unique<LaunchSessionRequest>(
                                     request_id, std::move(callback), clock_),
                                 launch_timeout)) {
    SendCastMessage(
        socket, CreateLaunchRequest(sender_id_, request_id, app_id, locale_));
  }
}

void CastMessageHandler::StopSession(int channel_id,
                                     const std::string& session_id,
                                     StopSessionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CastSocket* socket = socket_service_->GetSocket(channel_id);
  if (!socket) {
    DVLOG(2) << __func__ << ": socket not found: " << channel_id;
    return;
  }

  auto* requests = GetOrCreatePendingRequests(channel_id);
  int request_id = NextRequestId();
  DVLOG(2) << __func__ << ", channel_id: " << channel_id
           << ", request_id: " << request_id;
  if (requests->AddStopRequest(std::make_unique<StopSessionRequest>(
          request_id, std::move(callback), clock_))) {
    SendCastMessage(socket,
                    CreateStopRequest(sender_id_, request_id, session_id));
  }
}

void CastMessageHandler::SendAppMessage(int channel_id,
                                        const CastMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!IsCastInternalNamespace(message.namespace_()))
      << ": unexpected app message namespace: " << message.namespace_();

  CastSocket* socket = socket_service_->GetSocket(channel_id);
  if (!socket) {
    DVLOG(2) << __func__ << ": socket not found: " << channel_id;
    return;
  }

  SendCastMessage(socket, message);
}

void CastMessageHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CastMessageHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CastMessageHandler::OnError(const CastSocket& socket,
                                 ChannelError error_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int channel_id = socket.id();

  base::EraseIf(virtual_connections_,
                [&channel_id](const VirtualConnection& connection) {
                  return connection.channel_id == channel_id;
                });

  pending_requests_.erase(channel_id);
}

void CastMessageHandler::OnMessage(const CastSocket& socket,
                                   const CastMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__ << ", channel_id: " << socket.id()
           << ", message: " << CastMessageToString(message);
  if (IsCastInternalNamespace(message.namespace_())) {
    if (message.payload_type() ==
        cast_channel::CastMessage_PayloadType_STRING) {
      data_decoder::SafeJsonParser::ParseBatch(
          connector_.get(), message.payload_utf8(),
          base::BindRepeating(&CastMessageHandler::HandleCastInternalMessage,
                              weak_ptr_factory_.GetWeakPtr(), socket.id(),
                              message.source_id(), message.destination_id()),
          base::BindRepeating(&ReportParseError), data_decoder_batch_id_);
    } else {
      DLOG(ERROR) << "Dropping internal message with binary payload: "
                  << message.namespace_();
    }
  } else {
    DVLOG(2) << "Got app message from cast channel with namespace: "
             << message.namespace_();
    for (auto& observer : observers_)
      observer.OnAppMessage(socket.id(), message);
  }
}

void CastMessageHandler::HandleCastInternalMessage(
    int channel_id,
    const std::string& source_id,
    const std::string& destination_id,
    std::unique_ptr<base::Value> payload) {
  if (!payload->is_dict()) {
    ReportParseError("Parsed message not a dictionary");
    return;
  }

  // Check if the socket still exists as it might have been removed during
  // message parsing.
  if (!socket_service_->GetSocket(channel_id)) {
    DVLOG(2) << __func__ << ": socket not found: " << channel_id;
    return;
  }

  int request_id = 0;
  if (GetRequestIdFromResponse(*payload, &request_id) && request_id > 0) {
    auto requests_it = pending_requests_.find(channel_id);
    if (requests_it != pending_requests_.end())
      requests_it->second->HandlePendingRequest(request_id, *payload);
  }

  CastMessageType type = ParseMessageTypeFromPayload(*payload);
  if (type == CastMessageType::kOther) {
    DVLOG(2) << "Unknown message type: " << *payload;
    return;
  }

  if (type == CastMessageType::kCloseConnection) {
    // Source / destination is flipped.
    virtual_connections_.erase(
        VirtualConnection(channel_id, destination_id, source_id));
    return;
  }

  InternalMessage internal_message(type, std::move(*payload));
  for (auto& observer : observers_)
    observer.OnInternalMessage(channel_id, internal_message);
}

void CastMessageHandler::SendCastMessage(CastSocket* socket,
                                         const CastMessage& message) {
  // A virtual connection must be opened to the receiver before other messages
  // can be sent.
  DoEnsureConnection(socket, message.source_id(), message.destination_id());
  socket->transport()->SendMessage(
      message, base::Bind(&CastMessageHandler::OnMessageSent,
                          weak_ptr_factory_.GetWeakPtr()));
}

void CastMessageHandler::DoEnsureConnection(CastSocket* socket,
                                            const std::string& source_id,
                                            const std::string& destination_id) {
  VirtualConnection connection(socket->id(), source_id, destination_id);
  if (virtual_connections_.find(connection) != virtual_connections_.end())
    return;

  DVLOG(1) << "Creating VC for channel: " << connection.channel_id
           << ", source: " << connection.source_id
           << ", dest: " << connection.destination_id;
  CastMessage virtual_connection_request = CreateVirtualConnectionRequest(
      connection.source_id, connection.destination_id,
      connection.destination_id == kPlatformReceiverId
          ? VirtualConnectionType::kStrong
          : VirtualConnectionType::kInvisible,
      user_agent_, browser_version_);
  socket->transport()->SendMessage(
      virtual_connection_request, base::Bind(&CastMessageHandler::OnMessageSent,
                                             weak_ptr_factory_.GetWeakPtr()));

  // We assume the virtual connection request will succeed; otherwise this
  // will eventually self-correct.
  virtual_connections_.insert(connection);
}

void CastMessageHandler::OnMessageSent(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG_IF(2, result < 0) << "SendMessage failed with code: " << result;
}

CastMessageHandler::PendingRequests::PendingRequests() {}
CastMessageHandler::PendingRequests::~PendingRequests() {
  for (auto& request : pending_app_availability_requests_) {
    std::move(request.second->callback)
        .Run(request.second->app_id, GetAppAvailabilityResult::kUnknown);
  }

  if (pending_launch_session_request_) {
    LaunchSessionResponse response;
    response.result = LaunchSessionResponse::kError;
    std::move(pending_launch_session_request_->callback)
        .Run(std::move(response));
  }

  if (pending_stop_session_request_) {
    std::move(pending_stop_session_request_->callback).Run(false);
  }
}

bool CastMessageHandler::PendingRequests::AddAppAvailabilityRequest(
    std::unique_ptr<GetAppAvailabilityRequest> request) {
  std::string app_id = request->app_id;
  if (base::ContainsKey(pending_app_availability_requests_, request->app_id))
    return false;

  int request_id = request->request_id;
  request->timeout_timer.Start(
      FROM_HERE, kRequestTimeout,
      base::BindOnce(
          &CastMessageHandler::PendingRequests::AppAvailabilityTimedOut,
          base::Unretained(this), request_id));
  pending_app_availability_requests_.emplace(app_id, std::move(request));
  return true;
}

bool CastMessageHandler::PendingRequests::AddLaunchRequest(
    std::unique_ptr<LaunchSessionRequest> request,
    base::TimeDelta timeout) {
  if (pending_launch_session_request_)
    return false;

  int request_id = request->request_id;
  request->timeout_timer.Start(
      FROM_HERE, timeout,
      base::BindOnce(
          &CastMessageHandler::PendingRequests::LaunchSessionTimedOut,
          base::Unretained(this), request_id));
  pending_launch_session_request_ = std::move(request);
  return true;
}

bool CastMessageHandler::PendingRequests::AddStopRequest(
    std::unique_ptr<StopSessionRequest> request) {
  if (pending_stop_session_request_)
    return false;

  int request_id = request->request_id;
  request->timeout_timer.Start(
      FROM_HERE, kRequestTimeout,
      base::Bind(&CastMessageHandler::PendingRequests::StopSessionTimedOut,
                 base::Unretained(this), request_id));
  pending_stop_session_request_ = std::move(request);
  return true;
}

void CastMessageHandler::PendingRequests::HandlePendingRequest(
    int request_id,
    const base::Value& response) {
  auto app_availability_it =
      std::find_if(pending_app_availability_requests_.begin(),
                   pending_app_availability_requests_.end(),
                   [&request_id](const auto& entry) {
                     return entry.second->request_id == request_id;
                   });
  if (app_availability_it != pending_app_availability_requests_.end()) {
    GetAppAvailabilityResult result = GetAppAvailabilityResultFromResponse(
        response, app_availability_it->second->app_id);
    std::move(app_availability_it->second->callback)
        .Run(app_availability_it->second->app_id, result);
    pending_app_availability_requests_.erase(app_availability_it);
    return;
  }

  if (pending_launch_session_request_ &&
      pending_launch_session_request_->request_id == request_id) {
    std::move(pending_launch_session_request_->callback)
        .Run(GetLaunchSessionResponse(response));
    pending_launch_session_request_.reset();
    return;
  }

  if (pending_stop_session_request_ &&
      pending_stop_session_request_->request_id == request_id) {
    std::move(pending_stop_session_request_->callback).Run(true);
    pending_stop_session_request_.reset();
    return;
  }
}

void CastMessageHandler::PendingRequests::AppAvailabilityTimedOut(
    int request_id) {
  DVLOG(1) << __func__ << ", request_id: " << request_id;

  auto it = std::find_if(pending_app_availability_requests_.begin(),
                         pending_app_availability_requests_.end(),
                         [&request_id](const auto& entry) {
                           return entry.second->request_id == request_id;
                         });

  CHECK(it != pending_app_availability_requests_.end());
  std::move(it->second->callback)
      .Run(it->second->app_id, GetAppAvailabilityResult::kUnknown);
  pending_app_availability_requests_.erase(it);
}

void CastMessageHandler::PendingRequests::LaunchSessionTimedOut(
    int request_id) {
  DVLOG(1) << __func__ << ", request_id: " << request_id;
  CHECK(pending_launch_session_request_);
  CHECK(pending_launch_session_request_->request_id == request_id);

  LaunchSessionResponse response;
  response.result = LaunchSessionResponse::kTimedOut;
  std::move(pending_launch_session_request_->callback).Run(std::move(response));
  pending_launch_session_request_.reset();
}

void CastMessageHandler::PendingRequests::StopSessionTimedOut(int request_id) {
  DVLOG(1) << __func__ << ", request_id: " << request_id;
  CHECK(pending_stop_session_request_);
  CHECK(pending_stop_session_request_->request_id == request_id);

  std::move(pending_stop_session_request_->callback).Run(false);
  pending_stop_session_request_.reset();
}

}  // namespace cast_channel
