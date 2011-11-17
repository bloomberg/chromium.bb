// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/connection_to_client.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop_proxy.h"
#include "google/protobuf/message.h"
#include "net/base/io_buffer.h"
#include "remoting/protocol/client_control_sender.h"
#include "remoting/protocol/host_message_dispatcher.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_stub.h"

namespace remoting {
namespace protocol {

ConnectionToClient::ConnectionToClient(protocol::Session* session)
    : handler_(NULL),
      host_stub_(NULL),
      input_stub_(NULL),
      session_(session),
      control_connected_(false),
      input_connected_(false),
      video_connected_(false) {
  session_->SetStateChangeCallback(
      base::Bind(&ConnectionToClient::OnSessionStateChange,
                 base::Unretained(this)));
}

ConnectionToClient::~ConnectionToClient() {
  if (session_.get()) {
    base::MessageLoopProxy::current()->DeleteSoon(
        FROM_HERE, session_.release());
  }
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

  DCHECK(session_.get());
  Session* session = session_.release();

  // It may not be safe to delete |session_| here becase this method
  // may be invoked in resonse to a libjingle event and libjingle's
  // sigslot doesn't handle it properly, so postpone the deletion.
  base::MessageLoopProxy::current()->DeleteSoon(FROM_HERE, session);

  // This should trigger OnConnectionClosed() event and this object
  // may be destroyed as the result.
  session->Close();
}

void ConnectionToClient::UpdateSequenceNumber(int64 sequence_number) {
  DCHECK(CalledOnValidThread());
  handler_->OnSequenceNumberUpdated(this, sequence_number);
}

VideoStub* ConnectionToClient::video_stub() {
  DCHECK(CalledOnValidThread());
  return video_writer_.get();
}

// Return pointer to ClientStub.
ClientStub* ConnectionToClient::client_stub() {
  DCHECK(CalledOnValidThread());
  return client_control_sender_.get();
}

void ConnectionToClient::set_host_stub(protocol::HostStub* host_stub) {
  DCHECK(CalledOnValidThread());
  host_stub_ = host_stub;
}

void ConnectionToClient::set_input_stub(protocol::InputStub* input_stub) {
  DCHECK(CalledOnValidThread());
  input_stub_ = input_stub;
}

void ConnectionToClient::OnSessionStateChange(protocol::Session::State state) {
  DCHECK(CalledOnValidThread());

  DCHECK(handler_);
  switch(state) {
    case protocol::Session::CONNECTING:
      // Don't care about this message.
      break;

    case protocol::Session::CONNECTED:
      video_writer_.reset(
          VideoWriter::Create(base::MessageLoopProxy::current(),
                              session_->config()));
      video_writer_->Init(
          session_.get(), base::Bind(&ConnectionToClient::OnVideoInitialized,
                                     base::Unretained(this)));
      break;

    case protocol::Session::CONNECTED_CHANNELS:
      client_control_sender_.reset(
          new ClientControlSender(base::MessageLoopProxy::current(),
                                  session_->control_channel()));
      dispatcher_.reset(new HostMessageDispatcher());
      dispatcher_->Initialize(this, host_stub_, input_stub_);

      control_connected_ = true;
      input_connected_ = true;
      NotifyIfChannelsReady();
      break;

    case protocol::Session::CLOSED:
      CloseChannels();
      handler_->OnConnectionClosed(this);
      break;

    case protocol::Session::FAILED:
      CloseOnError();
      break;

    default:
      // We shouldn't receive other states.
      NOTREACHED();
  }
}

void ConnectionToClient::OnVideoInitialized(bool successful) {
  DCHECK(CalledOnValidThread());

  if (!successful) {
    LOG(ERROR) << "Failed to connect video channel";
    CloseOnError();
    return;
  }

  video_connected_ = true;
  NotifyIfChannelsReady();
}

void ConnectionToClient::NotifyIfChannelsReady() {
  DCHECK(CalledOnValidThread());

  if (control_connected_ && input_connected_ && video_connected_)
    handler_->OnConnectionOpened(this);
}

void ConnectionToClient::CloseOnError() {
  CloseChannels();
  handler_->OnConnectionFailed(this);
}

void ConnectionToClient::CloseChannels() {
  video_writer_.reset();
  client_control_sender_.reset();
  dispatcher_.reset();
}

}  // namespace protocol
}  // namespace remoting
