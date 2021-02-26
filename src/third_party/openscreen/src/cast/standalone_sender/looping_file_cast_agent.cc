// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_sender/looping_file_cast_agent.h"

#include <string>
#include <utility>
#include <vector>

#include "cast/standalone_sender/looping_file_sender.h"
#include "cast/streaming/capture_recommendations.h"
#include "cast/streaming/constants.h"
#include "cast/streaming/offer_messages.h"
#include "util/trace_logging.h"

namespace openscreen {
namespace cast {
namespace {

using DeviceMediaPolicy = SenderSocketFactory::DeviceMediaPolicy;

}  // namespace

LoopingFileCastAgent::LoopingFileCastAgent(TaskRunner* task_runner)
    : task_runner_(task_runner) {
  router_ = MakeSerialDelete<VirtualConnectionRouter>(task_runner_,
                                                      &connection_manager_);
  message_port_ =
      MakeSerialDelete<CastSocketMessagePort>(task_runner_, router_.get());
  socket_factory_ =
      MakeSerialDelete<SenderSocketFactory>(task_runner_, this, task_runner_);
  connection_factory_ = SerialDeletePtr<TlsConnectionFactory>(
      task_runner_,
      TlsConnectionFactory::CreateFactory(socket_factory_.get(), task_runner_)
          .release());
  socket_factory_->set_factory(connection_factory_.get());
}

LoopingFileCastAgent::~LoopingFileCastAgent() = default;

void LoopingFileCastAgent::Connect(ConnectionSettings settings) {
  TRACE_DEFAULT_SCOPED(TraceCategory::kStandaloneSender);
  connection_settings_ = std::move(settings);
  const auto policy = connection_settings_->should_include_video
                          ? DeviceMediaPolicy::kIncludesVideo
                          : DeviceMediaPolicy::kAudioOnly;

  task_runner_->PostTask([this, policy] {
    wake_lock_ = ScopedWakeLock::Create(task_runner_);
    socket_factory_->Connect(connection_settings_->receiver_endpoint, policy,
                             router_.get());
  });
}

void LoopingFileCastAgent::Stop() {
  task_runner_->PostTask([this] {
    StopCurrentSession();

    connection_factory_.reset();
    connection_settings_.reset();
    socket_factory_.reset();
    wake_lock_.reset();
  });
}

void LoopingFileCastAgent::OnConnected(SenderSocketFactory* factory,
                                       const IPEndpoint& endpoint,
                                       std::unique_ptr<CastSocket> socket) {
  TRACE_DEFAULT_SCOPED(TraceCategory::kStandaloneSender);
  if (current_session_) {
    OSP_LOG_WARN << "Already connected, dropping peer at: " << endpoint;
    return;
  }

  OSP_LOG_INFO << "Received connection from peer at: " << endpoint;
  message_port_->SetSocket(socket->GetWeakPtr());
  router_->TakeSocket(this, std::move(socket));
  CreateAndStartSession();
}

void LoopingFileCastAgent::OnError(SenderSocketFactory* factory,
                                   const IPEndpoint& endpoint,
                                   Error error) {
  OSP_LOG_ERROR << "Cast agent received socket factory error: " << error;
  StopCurrentSession();
}

void LoopingFileCastAgent::OnClose(CastSocket* cast_socket) {
  OSP_VLOG << "Cast agent socket closed.";
  StopCurrentSession();
}

void LoopingFileCastAgent::OnError(CastSocket* socket, Error error) {
  OSP_LOG_ERROR << "Cast agent received socket error: " << error;
  StopCurrentSession();
}

void LoopingFileCastAgent::OnNegotiated(
    const SenderSession* session,
    SenderSession::ConfiguredSenders senders,
    capture_recommendations::Recommendations capture_recommendations) {
  if (senders.audio_sender == nullptr || senders.video_sender == nullptr) {
    OSP_LOG_ERROR << "Missing either audio or video, so exiting...";
    return;
  }

  OSP_VLOG << "Successfully negotiated with sender.";

  file_sender_ = std::make_unique<LoopingFileSender>(
      task_runner_, connection_settings_->path_to_file.c_str(),
      connection_settings_->receiver_endpoint, senders,
      connection_settings_->max_bitrate);
}

// Currently, we just kill the session if an error is encountered.
void LoopingFileCastAgent::OnError(const SenderSession* session, Error error) {
  OSP_LOG_ERROR << "Cast agent received sender session error: " << error;
  StopCurrentSession();
}

void LoopingFileCastAgent::CreateAndStartSession() {
  TRACE_DEFAULT_SCOPED(TraceCategory::kStandaloneSender);
  environment_ =
      std::make_unique<Environment>(&Clock::now, task_runner_, IPEndpoint{});
  current_session_ = std::make_unique<SenderSession>(
      connection_settings_->receiver_endpoint.address, this, environment_.get(),
      message_port_.get());

  AudioCaptureConfig audio_config;
  VideoCaptureConfig video_config;
  // Use default display resolution of 1080P.
  video_config.resolutions.emplace_back(DisplayResolution{});

  OSP_VLOG << "Starting session negotiation.";
  const Error negotiation_error =
      current_session_->Negotiate({audio_config}, {video_config});
  if (!negotiation_error.ok()) {
    OSP_LOG_ERROR << "Failed to negotiate a session: " << negotiation_error;
  }
}

void LoopingFileCastAgent::StopCurrentSession() {
  current_session_.reset();
  environment_.reset();
  file_sender_.reset();
  router_->CloseSocket(message_port_->GetSocketId());
  message_port_->SetSocket(nullptr);
}

}  // namespace cast
}  // namespace openscreen
