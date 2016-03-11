// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_client.h"

#include <utility>

#include "remoting/base/capabilities.h"
#include "remoting/client/audio_decode_scheduler.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_user_interface.h"
#include "remoting/protocol/authenticator.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/ice_connection_to_host.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/video_renderer.h"
#include "remoting/protocol/webrtc_connection_to_host.h"

namespace remoting {

ChromotingClient::ChromotingClient(ClientContext* client_context,
                                   ClientUserInterface* user_interface,
                                   protocol::VideoRenderer* video_renderer,
                                   scoped_ptr<AudioPlayer> audio_player)
    : user_interface_(user_interface), video_renderer_(video_renderer) {
  DCHECK(client_context->main_task_runner()->BelongsToCurrentThread());
  if (audio_player) {
    audio_decode_scheduler_.reset(new AudioDecodeScheduler(
        client_context->main_task_runner(),
        client_context->audio_decode_task_runner(), std::move(audio_player)));
  }
}

ChromotingClient::~ChromotingClient() {
  if (signal_strategy_)
      signal_strategy_->RemoveListener(this);
}

void ChromotingClient::set_protocol_config(
    scoped_ptr<protocol::CandidateSessionConfig> config) {
  protocol_config_ = std::move(config);
}

void ChromotingClient::SetConnectionToHostForTests(
    scoped_ptr<protocol::ConnectionToHost> connection_to_host) {
  connection_ = std::move(connection_to_host);
}

void ChromotingClient::Start(
    SignalStrategy* signal_strategy,
    scoped_ptr<protocol::Authenticator> authenticator,
    scoped_refptr<protocol::TransportContext> transport_context,
    const std::string& host_jid,
    const std::string& capabilities) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!session_manager_);  // Start must be called more than once.

  host_jid_ = host_jid;
  local_capabilities_ = capabilities;

  if (!protocol_config_)
    protocol_config_ = protocol::CandidateSessionConfig::CreateDefault();
  if (!audio_decode_scheduler_)
    protocol_config_->DisableAudioChannel();

  if (!connection_) {
    if (protocol_config_->webrtc_supported()) {
      DCHECK(!protocol_config_->ice_supported());
#if defined(OS_NACL)
      LOG(FATAL) << "WebRTC is not supported in webapp.";
#else   // defined(OS_NACL)
      connection_.reset(new protocol::WebrtcConnectionToHost());
#endif  // !defined(OS_NACL)
    } else {
      DCHECK(protocol_config_->ice_supported());
      connection_.reset(new protocol::IceConnectionToHost());
    }
  }
  connection_->set_client_stub(this);
  connection_->set_clipboard_stub(this);
  connection_->set_video_renderer(video_renderer_);
  connection_->set_audio_stub(audio_decode_scheduler_.get());

  session_manager_.reset(new protocol::JingleSessionManager(signal_strategy));
  session_manager_->set_protocol_config(std::move(protocol_config_));

  authenticator_ = std::move(authenticator);
  transport_context_ = transport_context;

  signal_strategy_ = signal_strategy;
  signal_strategy_->AddListener(this);

  switch (signal_strategy_->GetState()) {
    case SignalStrategy::CONNECTING:
      // Nothing to do here. Just need to wait until |signal_strategy_| becomes
      // connected.
      break;
    case SignalStrategy::CONNECTED:
      StartConnection();
      break;
    case SignalStrategy::DISCONNECTED:
      signal_strategy_->Connect();
      break;
  }
}

void ChromotingClient::SetCapabilities(
    const protocol::Capabilities& capabilities) {
  DCHECK(thread_checker_.CalledOnValidThread());

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
      IntersectCapabilities(local_capabilities_, host_capabilities_));
}

void ChromotingClient::SetPairingResponse(
    const protocol::PairingResponse& pairing_response) {
  DCHECK(thread_checker_.CalledOnValidThread());

  user_interface_->SetPairingResponse(pairing_response);
}

void ChromotingClient::DeliverHostMessage(
    const protocol::ExtensionMessage& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  user_interface_->DeliverHostMessage(message);
}

void ChromotingClient::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  user_interface_->GetClipboardStub()->InjectClipboardEvent(event);
}

void ChromotingClient::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  DCHECK(thread_checker_.CalledOnValidThread());

  user_interface_->GetCursorShapeStub()->SetCursorShape(cursor_shape);
}

void ChromotingClient::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(thread_checker_.CalledOnValidThread());
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

void ChromotingClient::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state == SignalStrategy::CONNECTED) {
    VLOG(1) << "Connected as: " << signal_strategy_->GetLocalJid();
    // After signaling has been connected we can try connecting to the host.
    if (connection_ &&
        connection_->state() == protocol::ConnectionToHost::INITIALIZING) {
      StartConnection();
    }
  } else if (state == SignalStrategy::DISCONNECTED) {
    VLOG(1) << "Signaling connection closed.";
    connection_.reset();
    user_interface_->OnConnectionState(protocol::ConnectionToHost::CLOSED,
                                       protocol::SIGNALING_ERROR);
  }
}

bool ChromotingClient::OnSignalStrategyIncomingStanza(
    const buzz::XmlElement* stanza) {
  return false;
}

void ChromotingClient::StartConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  connection_->Connect(
      session_manager_->Connect(host_jid_, std::move(authenticator_)),
      transport_context_, this);
}

void ChromotingClient::OnAuthenticated() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Initialize the decoder.
  if (connection_->config().is_audio_enabled())
    audio_decode_scheduler_->Initialize(connection_->config());
}

void ChromotingClient::OnChannelsConnected() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Negotiate capabilities with the host.
  VLOG(1) << "Client capabilities: " << local_capabilities_;

  protocol::Capabilities capabilities;
  capabilities.set_capabilities(local_capabilities_);
  connection_->host_stub()->SetCapabilities(capabilities);
}

}  // namespace remoting
