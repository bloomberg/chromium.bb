// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_client.h"

#include "base/bind.h"
#include "remoting/client/audio_decode_scheduler.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_user_interface.h"
#include "remoting/client/rectangle_update_decoder.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/authentication_method.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/negotiating_authenticator.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport.h"

namespace remoting {

using protocol::AuthenticationMethod;

ChromotingClient::ChromotingClient(
    const ClientConfig& config,
    ClientContext* client_context,
    protocol::ConnectionToHost* connection,
    ClientUserInterface* user_interface,
    scoped_refptr<FrameConsumerProxy> frame_consumer,
    scoped_ptr<AudioPlayer> audio_player)
    : config_(config),
      task_runner_(client_context->main_task_runner()),
      connection_(connection),
      user_interface_(user_interface),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  rectangle_decoder_ =
      new RectangleUpdateDecoder(client_context->main_task_runner(),
                                 client_context->decode_task_runner(),
                                 frame_consumer);
  audio_decode_scheduler_.reset(new AudioDecodeScheduler(
      client_context->main_task_runner(),
      client_context->audio_decode_task_runner(),
      audio_player.Pass()));
}

ChromotingClient::~ChromotingClient() {
}

void ChromotingClient::Start(
    scoped_refptr<XmppProxy> xmpp_proxy,
    scoped_ptr<protocol::TransportFactory> transport_factory) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  scoped_ptr<protocol::Authenticator> authenticator(
      protocol::NegotiatingAuthenticator::CreateForClient(
          config_.authentication_tag, config_.fetch_secret_callback,
          config_.authentication_methods));

  // Create a WeakPtr to ourself for to use for all posted tasks.
  weak_ptr_ = weak_factory_.GetWeakPtr();

  connection_->Connect(xmpp_proxy, config_.local_jid, config_.host_jid,
                       config_.host_public_key, transport_factory.Pass(),
                       authenticator.Pass(), this, this, this,
                       rectangle_decoder_,
                       audio_decode_scheduler_.get());
}

void ChromotingClient::Stop(const base::Closure& shutdown_task) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  connection_->Disconnect(base::Bind(&ChromotingClient::OnDisconnected,
                                     weak_ptr_, shutdown_task));
}

FrameProducer* ChromotingClient::GetFrameProducer() {
  return rectangle_decoder_;
}

void ChromotingClient::OnDisconnected(const base::Closure& shutdown_task) {
  shutdown_task.Run();
}

ChromotingStats* ChromotingClient::GetStats() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return rectangle_decoder_->GetStats();
}

void ChromotingClient::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  user_interface_->GetClipboardStub()->InjectClipboardEvent(event);
}

void ChromotingClient::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  user_interface_->GetCursorShapeStub()->SetCursorShape(cursor_shape);
}

void ChromotingClient::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOG(1) << "ChromotingClient::OnConnectionState(" << state << ")";
  if (state == protocol::ConnectionToHost::CONNECTED)
    Initialize();
  user_interface_->OnConnectionState(state, error);
}

void ChromotingClient::OnConnectionReady(bool ready) {
  VLOG(1) << "ChromotingClient::OnConnectionReady(" << ready << ")";
  user_interface_->OnConnectionReady(ready);
}

void ChromotingClient::Initialize() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Initialize the decoder.
  rectangle_decoder_->Initialize(connection_->config());
  if (connection_->config().is_audio_enabled())
    audio_decode_scheduler_->Initialize(connection_->config());
}

}  // namespace remoting
