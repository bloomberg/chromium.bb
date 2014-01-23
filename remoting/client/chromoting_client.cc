// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_client.h"

#include "base/bind.h"
#include "remoting/base/capabilities.h"
#include "remoting/client/audio_decode_scheduler.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_user_interface.h"
#include "remoting/client/video_renderer.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/authentication_method.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/negotiating_client_authenticator.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport.h"

namespace remoting {

using protocol::AuthenticationMethod;

ChromotingClient::ChromotingClient(
    const ClientConfig& config,
    ClientContext* client_context,
    protocol::ConnectionToHost* connection,
    ClientUserInterface* user_interface,
    VideoRenderer* video_renderer,
    scoped_ptr<AudioPlayer> audio_player)
    : config_(config),
      task_runner_(client_context->main_task_runner()),
      connection_(connection),
      user_interface_(user_interface),
      video_renderer_(video_renderer),
      host_capabilities_received_(false),
      weak_factory_(this) {
  if (audio_player) {
    audio_decode_scheduler_.reset(new AudioDecodeScheduler(
        client_context->main_task_runner(),
        client_context->audio_decode_task_runner(),
        audio_player.Pass()));
  }
}

ChromotingClient::~ChromotingClient() {
}

void ChromotingClient::Start(
    SignalStrategy* signal_strategy,
    scoped_ptr<protocol::TransportFactory> transport_factory) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  scoped_ptr<protocol::Authenticator> authenticator(
      new protocol::NegotiatingClientAuthenticator(
          config_.client_pairing_id,
          config_.client_paired_secret,
          config_.authentication_tag,
          config_.fetch_secret_callback,
          user_interface_->GetTokenFetcher(config_.host_public_key),
          config_.authentication_methods));

  // Create a WeakPtr to ourself for to use for all posted tasks.
  weak_ptr_ = weak_factory_.GetWeakPtr();

  connection_->Connect(signal_strategy,
                       config_.host_jid,
                       config_.host_public_key,
                       transport_factory.Pass(),
                       authenticator.Pass(),
                       this,
                       this,
                       this,
                       video_renderer_,
                       audio_decode_scheduler_.get());
}

void ChromotingClient::SetCapabilities(
    const protocol::Capabilities& capabilities) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Only accept the first |protocol::Capabilities| message.
  if (host_capabilities_received_) {
    LOG(WARNING) << "protocol::Capabilities has been received already.";
    return;
  }

  host_capabilities_received_ = true;
  if (capabilities.has_capabilities())
    host_capabilities_ = capabilities.capabilities();

  VLOG(1) << "Host capabilities: " << host_capabilities_;

  // Calculate the set of capabilities enabled by both client and host and pass
  // it to the webapp.
  user_interface_->SetCapabilities(
      IntersectCapabilities(config_.capabilities, host_capabilities_));
}

void ChromotingClient::SetPairingResponse(
    const protocol::PairingResponse& pairing_response) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  user_interface_->SetPairingResponse(pairing_response);
}

void ChromotingClient::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  user_interface_->DeliverHostMessage(message);
}

void ChromotingClient::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  user_interface_->GetClipboardStub()->InjectClipboardEvent(event);
}

void ChromotingClient::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  user_interface_->GetCursorShapeStub()->SetCursorShape(cursor_shape);
}

void ChromotingClient::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOG(1) << "ChromotingClient::OnConnectionState(" << state << ")";

  if (state == protocol::ConnectionToHost::AUTHENTICATED) {
    OnAuthenticated();
  } else if (state == protocol::ConnectionToHost::CONNECTED) {
    OnChannelsConnected();
  }
  user_interface_->OnConnectionState(state, error);
}

void ChromotingClient::OnConnectionReady(bool ready) {
  VLOG(1) << "ChromotingClient::OnConnectionReady(" << ready << ")";
  user_interface_->OnConnectionReady(ready);
}

void ChromotingClient::OnRouteChanged(const std::string& channel_name,
                                      const protocol::TransportRoute& route) {
  VLOG(0) << "Using " << protocol::TransportRoute::GetTypeString(route.type)
          << " connection for " << channel_name << " channel";
  user_interface_->OnRouteChanged(channel_name, route);
}

void ChromotingClient::OnAuthenticated() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Initialize the decoder.
  video_renderer_->Initialize(connection_->config());
  if (connection_->config().is_audio_enabled())
    audio_decode_scheduler_->Initialize(connection_->config());

  // Do not negotiate capabilities with the host if the host does not support
  // them.
  if (!connection_->config().SupportsCapabilities()) {
    VLOG(1) << "The host does not support any capabilities.";

    host_capabilities_received_ = true;
    user_interface_->SetCapabilities(host_capabilities_);
  }
}

void ChromotingClient::OnChannelsConnected() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Negotiate capabilities with the host.
  if (connection_->config().SupportsCapabilities()) {
    VLOG(1) << "Client capabilities: " << config_.capabilities;

    protocol::Capabilities capabilities;
    capabilities.set_capabilities(config_.capabilities);
    connection_->host_stub()->SetCapabilities(capabilities);
  }
}

}  // namespace remoting
