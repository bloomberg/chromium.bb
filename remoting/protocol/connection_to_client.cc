// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/connection_to_client.h"

#include "base/bind.h"
#include "base/location.h"
#include "net/base/io_buffer.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/host_control_dispatcher.h"
#include "remoting/protocol/host_event_dispatcher.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/host_video_dispatcher.h"
#include "remoting/protocol/input_stub.h"

namespace remoting {
namespace protocol {

ConnectionToClient::ConnectionToClient(protocol::Session* session)
    : handler_(nullptr),
      session_(session) {
  session_->SetEventHandler(this);
}

ConnectionToClient::~ConnectionToClient() {
}

void ConnectionToClient::SetEventHandler(EventHandler* event_handler) {
  DCHECK(CalledOnValidThread());
  handler_ = event_handler;
}

protocol::Session* ConnectionToClient::session() {
  DCHECK(CalledOnValidThread());
  return session_.get();
}

void ConnectionToClient::Disconnect(ErrorCode error) {
  DCHECK(CalledOnValidThread());

  CloseChannels();

  // This should trigger OnConnectionClosed() event and this object
  // may be destroyed as the result.
  session_->Close(error);
}

void ConnectionToClient::OnInputEventReceived(int64_t timestamp) {
  DCHECK(CalledOnValidThread());
  handler_->OnInputEventReceived(this, timestamp);
}

VideoStub* ConnectionToClient::video_stub() {
  DCHECK(CalledOnValidThread());
  return video_dispatcher_.get();
}

AudioStub* ConnectionToClient::audio_stub() {
  DCHECK(CalledOnValidThread());
  return audio_writer_.get();
}

// Return pointer to ClientStub.
ClientStub* ConnectionToClient::client_stub() {
  DCHECK(CalledOnValidThread());
  return control_dispatcher_.get();
}

void ConnectionToClient::set_clipboard_stub(
    protocol::ClipboardStub* clipboard_stub) {
  DCHECK(CalledOnValidThread());
  control_dispatcher_->set_clipboard_stub(clipboard_stub);
}

void ConnectionToClient::set_host_stub(protocol::HostStub* host_stub) {
  DCHECK(CalledOnValidThread());
  control_dispatcher_->set_host_stub(host_stub);
}

void ConnectionToClient::set_input_stub(protocol::InputStub* input_stub) {
  DCHECK(CalledOnValidThread());
  event_dispatcher_->set_input_stub(input_stub);
}

void ConnectionToClient::set_video_feedback_stub(
    VideoFeedbackStub* video_feedback_stub) {
  DCHECK(CalledOnValidThread());
  video_dispatcher_->set_video_feedback_stub(video_feedback_stub);
}

void ConnectionToClient::OnSessionStateChange(Session::State state) {
  DCHECK(CalledOnValidThread());

  DCHECK(handler_);
  switch(state) {
    case Session::INITIALIZING:
    case Session::CONNECTING:
    case Session::ACCEPTING:
    case Session::CONNECTED:
      // Don't care about these events.
      break;
    case Session::AUTHENTICATING:
      handler_->OnConnectionAuthenticating(this);
      break;
    case Session::AUTHENTICATED:
      // Initialize channels.
      control_dispatcher_.reset(new HostControlDispatcher());
      control_dispatcher_->Init(session_.get(),
                                session_->config().control_config(), this);

      event_dispatcher_.reset(new HostEventDispatcher());
      event_dispatcher_->Init(session_.get(), session_->config().event_config(),
                              this);
      event_dispatcher_->set_on_input_event_callback(base::Bind(
          &ConnectionToClient::OnInputEventReceived, base::Unretained(this)));

      video_dispatcher_.reset(new HostVideoDispatcher());
      video_dispatcher_->Init(session_.get(), session_->config().video_config(),
                              this);

      audio_writer_ = AudioWriter::Create(session_->config());
      if (audio_writer_.get()) {
        audio_writer_->Init(session_.get(), session_->config().audio_config(),
                            this);
      }

      // Notify the handler after initializing the channels, so that
      // ClientSession can get a client clipboard stub.
      handler_->OnConnectionAuthenticated(this);
      break;

    case Session::CLOSED:
      Close(OK);
      break;

    case Session::FAILED:
      Close(session_->error());
      break;
  }
}

void ConnectionToClient::OnSessionRouteChange(
    const std::string& channel_name,
    const TransportRoute& route) {
  handler_->OnRouteChange(this, channel_name, route);
}

void ConnectionToClient::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  DCHECK(CalledOnValidThread());

  NotifyIfChannelsReady();
}

void ConnectionToClient::OnChannelError(
    ChannelDispatcherBase* channel_dispatcher,
    ErrorCode error) {
  DCHECK(CalledOnValidThread());

  LOG(ERROR) << "Failed to connect channel "
             << channel_dispatcher->channel_name();
  Close(CHANNEL_CONNECTION_ERROR);
}

void ConnectionToClient::NotifyIfChannelsReady() {
  DCHECK(CalledOnValidThread());

  if (!control_dispatcher_ || !control_dispatcher_->is_connected())
    return;
  if (!event_dispatcher_ || !event_dispatcher_->is_connected())
    return;
  if (!video_dispatcher_ || !video_dispatcher_->is_connected())
    return;
  if ((!audio_writer_ || !audio_writer_->is_connected()) &&
      session_->config().is_audio_enabled()) {
    return;
  }
  handler_->OnConnectionChannelsConnected(this);
}

void ConnectionToClient::Close(ErrorCode error) {
  CloseChannels();
  handler_->OnConnectionClosed(this, error);
}

void ConnectionToClient::CloseChannels() {
  control_dispatcher_.reset();
  event_dispatcher_.reset();
  video_dispatcher_.reset();
  audio_writer_.reset();
}

}  // namespace protocol
}  // namespace remoting
