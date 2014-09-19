// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/connection_to_client.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
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
    : handler_(NULL),
      clipboard_stub_(NULL),
      host_stub_(NULL),
      input_stub_(NULL),
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

void ConnectionToClient::Disconnect() {
  DCHECK(CalledOnValidThread());

  CloseChannels();

  // This should trigger OnConnectionClosed() event and this object
  // may be destroyed as the result.
  session_->Close();
}

void ConnectionToClient::UpdateSequenceNumber(int64 sequence_number) {
  DCHECK(CalledOnValidThread());
  handler_->OnSequenceNumberUpdated(this, sequence_number);
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
  clipboard_stub_ = clipboard_stub;
}

ClipboardStub* ConnectionToClient::clipboard_stub() {
  DCHECK(CalledOnValidThread());
  return clipboard_stub_;
}

void ConnectionToClient::set_host_stub(protocol::HostStub* host_stub) {
  DCHECK(CalledOnValidThread());
  host_stub_ = host_stub;
}

HostStub* ConnectionToClient::host_stub() {
  DCHECK(CalledOnValidThread());
  return host_stub_;
}

void ConnectionToClient::set_input_stub(protocol::InputStub* input_stub) {
  DCHECK(CalledOnValidThread());
  input_stub_ = input_stub;
}

InputStub* ConnectionToClient::input_stub() {
  DCHECK(CalledOnValidThread());
  return input_stub_;
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
      control_dispatcher_->Init(
          session_.get(), session_->config().control_config(),
          base::Bind(&ConnectionToClient::OnChannelInitialized,
                     base::Unretained(this)));
      control_dispatcher_->set_clipboard_stub(clipboard_stub_);
      control_dispatcher_->set_host_stub(host_stub_);

      event_dispatcher_.reset(new HostEventDispatcher());
      event_dispatcher_->Init(
          session_.get(), session_->config().event_config(),
          base::Bind(&ConnectionToClient::OnChannelInitialized,
                     base::Unretained(this)));
      event_dispatcher_->set_input_stub(input_stub_);
      event_dispatcher_->set_sequence_number_callback(base::Bind(
          &ConnectionToClient::UpdateSequenceNumber, base::Unretained(this)));

      video_dispatcher_.reset(new HostVideoDispatcher());
      video_dispatcher_->Init(
          session_.get(), session_->config().video_config(),
          base::Bind(&ConnectionToClient::OnChannelInitialized,
                     base::Unretained(this)));

      audio_writer_ = AudioWriter::Create(session_->config());
      if (audio_writer_.get()) {
        audio_writer_->Init(
            session_.get(), session_->config().audio_config(),
            base::Bind(&ConnectionToClient::OnChannelInitialized,
                       base::Unretained(this)));
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

void ConnectionToClient::OnChannelInitialized(bool successful) {
  DCHECK(CalledOnValidThread());

  if (!successful) {
    LOG(ERROR) << "Failed to connect a channel";
    Close(CHANNEL_CONNECTION_ERROR);
    return;
  }

  NotifyIfChannelsReady();
}

void ConnectionToClient::NotifyIfChannelsReady() {
  DCHECK(CalledOnValidThread());

  if (!control_dispatcher_.get() || !control_dispatcher_->is_connected())
    return;
  if (!event_dispatcher_.get() || !event_dispatcher_->is_connected())
    return;
  if (!video_dispatcher_.get() || !video_dispatcher_->is_connected())
    return;
  if ((!audio_writer_.get() || !audio_writer_->is_connected()) &&
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
