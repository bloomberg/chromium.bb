// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/public/presentation/presentation_receiver.h"

#include <algorithm>
#include <memory>

#include "api/impl/presentation/presentation_common.h"
#include "api/public/message_demuxer.h"
#include "api/public/network_service_manager.h"
#include "api/public/protocol_connection_server.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"
#include "platform/api/time.h"

namespace openscreen {
namespace presentation {
namespace {

msgs::PresentationTerminationEvent_reason GetEventReason(
    TerminationReason reason) {
  switch (reason) {
    case TerminationReason::kReceiverUserTerminated:
      return msgs::kUserViaReceiver;
    case TerminationReason::kReceiverShuttingDown:
      return msgs::kReceiverShuttingDown;
    case TerminationReason::kReceiverPresentationUnloaded:
      return msgs::kUnloaded;
    case TerminationReason::kReceiverPresentationReplaced:
      return msgs::kNewReplacingCurrent;
    case TerminationReason::kReceiverIdleTooLong:
      return msgs::kIdleTooLong;
    case TerminationReason::kReceiverError:
      return msgs::kReceiver;
    case TerminationReason::kReceiverTerminateCalled:  // fallthrough
    default:
      return msgs::kTerminate;
  }
}

Error WritePresentationInitiationResponse(
    const msgs::PresentationInitiationResponse& response,
    ProtocolConnection* connection) {
  return connection->WriteMessage(response,
                                  msgs::EncodePresentationInitiationResponse);
}

Error WritePresentationConnectionOpenResponse(
    const msgs::PresentationConnectionOpenResponse& response,
    ProtocolConnection* connection) {
  return connection->WriteMessage(
      response, msgs::EncodePresentationConnectionOpenResponse);
}

Error WritePresentationTerminationEvent(
    const msgs::PresentationTerminationEvent& event,
    ProtocolConnection* connection) {
  return connection->WriteMessage(event,
                                  msgs::EncodePresentationTerminationEvent);
}

Error WritePresentationTerminationResponse(
    const msgs::PresentationTerminationResponse& response,
    ProtocolConnection* connection) {
  return connection->WriteMessage(response,
                                  msgs::EncodePresentationTerminationResponse);
}

Error WritePresentationUrlAvailabilityResponse(
    const msgs::PresentationUrlAvailabilityResponse& response,
    ProtocolConnection* connection) {
  return connection->WriteMessage(
      response, msgs::EncodePresentationUrlAvailabilityResponse);
}

}  // namespace

ErrorOr<size_t> Receiver::OnStreamMessage(uint64_t endpoint_id,
                                          uint64_t connection_id,
                                          msgs::Type message_type,
                                          const uint8_t* buffer,
                                          size_t buffer_size,
                                          platform::TimeDelta now) {
  switch (message_type) {
    case msgs::Type::kPresentationUrlAvailabilityRequest: {
      OSP_VLOG << "got presentation-url-availability-request";
      msgs::PresentationUrlAvailabilityRequest request;
      ssize_t decode_result = msgs::DecodePresentationUrlAvailabilityRequest(
          buffer, buffer_size, &request);
      if (decode_result < 0) {
        OSP_LOG_WARN << "Presentation-url-availability-request parse error: "
                     << decode_result;
        return Error::Code::kParseError;
      }

      msgs::PresentationUrlAvailabilityResponse response;
      response.request_id = request.request_id;

      response.url_availabilities = delegate_->OnUrlAvailabilityRequest(
          request.watch_id, request.watch_duration, std::move(request.urls));
      msgs::CborEncodeBuffer buffer;

      WritePresentationUrlAvailabilityResponse(
          response, GetProtocolConnection(endpoint_id).get());
      return decode_result;
    }

    case msgs::Type::kPresentationInitiationRequest: {
      OSP_VLOG << "got presentation-initiation-request";
      msgs::PresentationInitiationRequest request;
      const ssize_t result = msgs::DecodePresentationInitiationRequest(
          buffer, buffer_size, &request);
      if (result < 0) {
        OSP_LOG_WARN << "Presentation-initiation-request parse error: "
                     << result;
        return Error::Code::kParseError;
      }

      OSP_LOG << "Got an initiation request for: " << request.url;

      PresentationID presentation_id(std::move(request.presentation_id));
      if (!presentation_id) {
        msgs::PresentationInitiationResponse response;
        response.request_id = request.request_id;

        // TODO(jophba): remove decltype? Can we do it in generated code?
        response.result = static_cast<decltype(response.result)>(
            msgs::kInvalidPresentationId);
        Error write_error = WritePresentationInitiationResponse(
            response, GetProtocolConnection(endpoint_id).get());

        if (!write_error.ok())
          return write_error;
        return result;
      }

      auto& response_list = queued_responses_[presentation_id];
      QueuedResponse queued_response{
          /* .type = */ QueuedResponse::Type::kInitiation,
          /* .request_id = */ request.request_id,
          /* .connection_id = */ request.connection_id,
          /* .endpoint_id = */ endpoint_id};
      response_list.push_back(std::move(queued_response));

      const bool starting = delegate_->StartPresentation(
          Connection::PresentationInfo{presentation_id, request.url},
          endpoint_id, request.headers);

      if (starting)
        return result;

      queued_responses_.erase(presentation_id);
      msgs::PresentationInitiationResponse response;
      response.request_id = request.request_id;
      response.result =
          static_cast<decltype(response.result)>(msgs::kUnknownError);
      Error write_error = WritePresentationInitiationResponse(
          response, GetProtocolConnection(endpoint_id).get());
      if (!write_error.ok())
        return write_error;
      return result;
    }

    case msgs::Type::kPresentationConnectionOpenRequest: {
      OSP_VLOG << "Got a presentation-connection-open-request";
      msgs::PresentationConnectionOpenRequest request;
      const ssize_t result = msgs::DecodePresentationConnectionOpenRequest(
          buffer, buffer_size, &request);
      if (result < 0) {
        OSP_LOG_WARN << "Presentation-connection-open-request parse error: "
                     << result;
        return Error::Code::kParseError;
      }

      PresentationID presentation_id(std::move(request.presentation_id));

      // TODO(jophba): add logic to queue presentation connection open
      // (and terminate connection)
      // requests to check against when a presentation starts, in case
      // we get a request right before the beginning of the presentation.
      if (!presentation_id || started_presentations_.find(presentation_id) ==
                                  started_presentations_.end()) {
        msgs::PresentationConnectionOpenResponse response;
        response.request_id = request.request_id;
        response.result = static_cast<decltype(response.result)>(
            msgs::kUnknownPresentationId);
        Error write_error = WritePresentationConnectionOpenResponse(
            response, GetProtocolConnection(endpoint_id).get());
        if (!write_error.ok())
          return write_error;
        return result;
      }

      // TODO(btolsch): We would also check that connection_id isn't already
      // requested/in use but since the spec has already shifted to a
      // receiver-chosen connection ID, we'll ignore that until we change our
      // CDDL messages.
      std::vector<QueuedResponse>& responses =
          queued_responses_[presentation_id];
      responses.emplace_back(
          QueuedResponse{QueuedResponse::Type::kConnection, request.request_id,
                         request.connection_id, endpoint_id});
      bool connecting = delegate_->ConnectToPresentation(
          request.request_id, presentation_id, endpoint_id);
      if (connecting)
        return result;

      responses.pop_back();
      if (responses.empty())
        queued_responses_.erase(presentation_id);

      msgs::PresentationConnectionOpenResponse response;
      response.request_id = request.request_id;
      response.result =
          static_cast<decltype(response.result)>(msgs::kUnknownError);
      Error write_error = WritePresentationConnectionOpenResponse(
          response, GetProtocolConnection(endpoint_id).get());
      if (!write_error.ok())
        return write_error;
      return result;
    }

    case msgs::Type::kPresentationTerminationRequest: {
      OSP_VLOG << "got presentation-termination-open-request";
      msgs::PresentationTerminationRequest request;
      const ssize_t result = msgs::DecodePresentationTerminationRequest(
          buffer, buffer_size, &request);
      if (result < 0) {
        OSP_LOG_WARN << "Presentation-termination-request parse error: "
                     << result;
        return Error::Code::kParseError;
      }

      PresentationID presentation_id(std::move(request.presentation_id));
      OSP_LOG << "Got termination request for: " << presentation_id;

      auto presentation_entry = started_presentations_.end();
      if (presentation_id) {
        presentation_entry = started_presentations_.find(presentation_id);
      }

      if (presentation_entry != started_presentations_.end()) {
        msgs::PresentationTerminationResponse response;
        response.request_id = request.request_id;
        response.result = static_cast<decltype(response.result)>(
            msgs::kInvalidPresentationId);
        Error write_error = WritePresentationTerminationResponse(
            response, GetProtocolConnection(endpoint_id).get());
        if (!write_error.ok())
          return write_error;
        return result;
      }

      TerminationReason reason =
          (request.reason == msgs::kTerminatedByController)
              ? TerminationReason::kControllerTerminateCalled
              : TerminationReason::kControllerUserTerminated;
      presentation_entry->second.terminate_request_id = request.request_id;
      delegate_->TerminatePresentation(presentation_id, reason);

      return result;
    }

    default:
      return Error::Code::kUnknownMessageType;
  }
}

// TODO(issue/31): Remove singletons in the embedder API and protocol
// implementation layers and in presentation_connection, as well as unit tests.
// static
Receiver* Receiver::Get() {
  static Receiver& receiver = *new Receiver();
  return &receiver;
}

void Receiver::Init() {
  if (!connection_manager_) {
    connection_manager_ =
        std::make_unique<ConnectionManager>(GetServerDemuxer());
  }
}

void Receiver::Deinit() {
  connection_manager_.reset();
}

void Receiver::SetReceiverDelegate(ReceiverDelegate* delegate) {
  OSP_DCHECK(!delegate_ || !delegate);
  delegate_ = delegate;

  MessageDemuxer* demuxer = GetServerDemuxer();
  if (delegate_) {
    availability_watch_ = demuxer->SetDefaultMessageTypeWatch(
        msgs::Type::kPresentationUrlAvailabilityRequest, this);
    initiation_watch_ = demuxer->SetDefaultMessageTypeWatch(
        msgs::Type::kPresentationInitiationRequest, this);
    connection_watch_ = demuxer->SetDefaultMessageTypeWatch(
        msgs::Type::kPresentationConnectionOpenRequest, this);
    return;
  }

  StopWatching(&availability_watch_);
  StopWatching(&initiation_watch_);
  StopWatching(&connection_watch_);

  std::vector<std::string> presentations_to_remove(
      started_presentations_.size());
  for (auto& it : started_presentations_) {
    presentations_to_remove.push_back(it.first);
  }

  for (auto& presentation_id : presentations_to_remove) {
    OnPresentationTerminated(presentation_id,
                             TerminationReason::kReceiverShuttingDown);
  }
}

Error Receiver::OnPresentationStarted(const std::string& presentation_id,
                                      Connection* connection,
                                      ResponseResult result) {
  auto queued_responses_entry = queued_responses_.find(presentation_id);
  if (queued_responses_entry == queued_responses_.end())
    return Error::Code::kNoStartedPresentation;

  auto& responses = queued_responses_entry->second;
  if ((responses.size() != 1) ||
      (responses.front().type != QueuedResponse::Type::kInitiation)) {
    return Error::Code::kPresentationAlreadyStarted;
  }

  QueuedResponse& initiation_response = responses.front();
  msgs::PresentationInitiationResponse response;
  response.request_id = initiation_response.request_id;
  auto protocol_connection =
      GetProtocolConnection(initiation_response.endpoint_id);
  auto* raw_protocol_connection_ptr = protocol_connection.get();

  OSP_VLOG << "presentation started with protocol_connection id: "
           << protocol_connection->id();
  if (result != ResponseResult::kSuccess) {
    response.result =
        static_cast<decltype(response.result)>(msgs::kUnknownError);

    queued_responses_.erase(queued_responses_entry);
    return WritePresentationInitiationResponse(response,
                                               raw_protocol_connection_ptr);
  }

  response.result = static_cast<decltype(response.result)>(msgs::kSuccess);
  response.has_connection_result = true;
  response.connection_result =
      static_cast<decltype(response.connection_result)>(msgs::kSuccess);

  Presentation& presentation = started_presentations_[presentation_id];
  presentation.endpoint_id = initiation_response.endpoint_id;
  connection->OnConnected(initiation_response.connection_id,
                          initiation_response.endpoint_id,
                          std::move(protocol_connection));
  presentation.connections.push_back(connection);
  connection_manager_->AddConnection(connection);

  presentation.terminate_watch = GetServerDemuxer()->WatchMessageType(
      initiation_response.endpoint_id,
      msgs::Type::kPresentationTerminationRequest, this);

  queued_responses_.erase(queued_responses_entry);
  return WritePresentationInitiationResponse(response,
                                             raw_protocol_connection_ptr);
}

Error Receiver::OnConnectionCreated(uint64_t request_id,
                                    Connection* connection,
                                    ResponseResult result) {
  const auto presentation_id = connection->get_presentation_info().id;

  ErrorOr<QueuedResponseIterator> connection_response =
      GetQueuedResponse(presentation_id, request_id);
  if (connection_response.is_error()) {
    return connection_response.error();
  }
  connection->OnConnected(
      connection_response.value()->connection_id,
      connection_response.value()->endpoint_id,
      NetworkServiceManager::Get()
          ->GetProtocolConnectionServer()
          ->CreateProtocolConnection(connection_response.value()->endpoint_id));

  started_presentations_[presentation_id].connections.push_back(connection);
  connection_manager_->AddConnection(connection);

  msgs::PresentationConnectionOpenResponse response;
  response.request_id = request_id;
  response.result = static_cast<decltype(response.result)>(msgs::kSuccess);

  auto protocol_connection =
      GetProtocolConnection(connection_response.value()->endpoint_id);

  WritePresentationConnectionOpenResponse(response, protocol_connection.get());

  DeleteQueuedResponse(presentation_id, connection_response.value());
  return Error::None();
}

Error Receiver::OnPresentationTerminated(const std::string& presentation_id,
                                         TerminationReason reason) {
  auto presentation_entry = started_presentations_.find(presentation_id);
  if (presentation_entry == started_presentations_.end())
    return Error::Code::kNoStartedPresentation;

  Presentation& presentation = presentation_entry->second;
  presentation.terminate_watch = MessageDemuxer::MessageWatch();
  std::unique_ptr<ProtocolConnection> protocol_connection =
      GetProtocolConnection(presentation.endpoint_id);

  if (!protocol_connection)
    return Error::Code::kNoActiveConnection;

  for (auto* connection : presentation.connections)
    connection->OnTerminatedByRemote();

  if (presentation.terminate_request_id) {
    // TODO(btolsch): Also timeout if this point isn't reached.
    msgs::PresentationTerminationResponse response;
    response.request_id = presentation.terminate_request_id;
    response.result = static_cast<decltype(response.result)>(msgs::kSuccess);
    started_presentations_.erase(presentation_entry);
    return WritePresentationTerminationResponse(response,
                                                protocol_connection.get());
  }

  // TODO(btolsch): Same request/event question as connection-close.
  msgs::PresentationTerminationEvent event;
  event.presentation_id = presentation_id;
  event.reason = GetEventReason(reason);
  started_presentations_.erase(presentation_entry);
  return WritePresentationTerminationEvent(event, protocol_connection.get());
}

void Receiver::OnConnectionDestroyed(Connection* connection) {
  auto presentation_entry =
      started_presentations_.find(connection->get_presentation_info().id);
  if (presentation_entry == started_presentations_.end())
    return;

  std::vector<Connection*>& connections =
      presentation_entry->second.connections;

  auto past_the_end =
      std::remove(connections.begin(), connections.end(), connection);
  // An additional call to "erase" is necessary to actually adjust the size
  // of the vector.
  connections.erase(past_the_end, connections.end());

  connection_manager_->RemoveConnection(connection);
}

Receiver::Receiver() = default;

Receiver::~Receiver() = default;

void Receiver::DeleteQueuedResponse(const std::string& presentation_id,
                                    Receiver::QueuedResponseIterator response) {
  auto entry = queued_responses_.find(presentation_id);
  entry->second.erase(response);
  if (entry->second.empty())
    queued_responses_.erase(entry);
}

ErrorOr<Receiver::QueuedResponseIterator> Receiver::GetQueuedResponse(
    const std::string& presentation_id,
    uint64_t request_id) const {
  auto entry = queued_responses_.find(presentation_id);
  if (entry == queued_responses_.end()) {
    OSP_LOG_WARN << "connection created for unknown request";
    return Error::Code::kUnknownRequestId;
  }

  const std::vector<QueuedResponse>& responses = entry->second;
  Receiver::QueuedResponseIterator it =
      std::find_if(responses.begin(), responses.end(),
                   [request_id](const QueuedResponse& response) {
                     return response.request_id == request_id;
                   });

  if (it == responses.end()) {
    OSP_LOG_WARN << "connection created for unknown request";
    return Error::Code::kUnknownRequestId;
  }

  return it;
}

}  // namespace presentation
}  // namespace openscreen
