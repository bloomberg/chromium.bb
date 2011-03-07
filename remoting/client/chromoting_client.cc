// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_client.h"

#include "base/message_loop.h"
#include "remoting/base/tracer.h"
#include "remoting/client/chromoting_view.h"
#include "remoting/client/client_context.h"
#include "remoting/client/input_handler.h"
#include "remoting/client/rectangle_update_decoder.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/session_config.h"

namespace remoting {

ChromotingClient::ChromotingClient(const ClientConfig& config,
                                   ClientContext* context,
                                   protocol::ConnectionToHost* connection,
                                   ChromotingView* view,
                                   RectangleUpdateDecoder* rectangle_decoder,
                                   InputHandler* input_handler,
                                   CancelableTask* client_done)
    : config_(config),
      context_(context),
      connection_(connection),
      view_(view),
      rectangle_decoder_(rectangle_decoder),
      input_handler_(input_handler),
      client_done_(client_done),
      state_(CREATED),
      packet_being_processed_(false) {
}

ChromotingClient::~ChromotingClient() {
}

void ChromotingClient::Start() {
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingClient::Start));
    return;
  }

  connection_->Connect(config_.username, config_.auth_token, config_.host_jid,
                       this, this, this);

  if (!view_->Initialize()) {
    ClientDone();
  }
}

void ChromotingClient::StartSandboxed(scoped_refptr<XmppProxy> xmpp_proxy,
                                      const std::string& your_jid,
                                      const std::string& host_jid) {
  // TODO(ajwong): Merge this with Start(), and just change behavior based on
  // ClientConfig.
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingClient::StartSandboxed, xmpp_proxy,
                          your_jid, host_jid));
    return;
  }

  connection_->ConnectSandboxed(xmpp_proxy, your_jid, host_jid, this, this,
                                this);

  if (!view_->Initialize()) {
    ClientDone();
  }
}

void ChromotingClient::Stop() {
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingClient::Stop));
    return;
  }

  connection_->Disconnect();

  view_->TearDown();
}

void ChromotingClient::ClientDone() {
  if (client_done_ != NULL) {
    message_loop()->PostTask(FROM_HERE, client_done_);
  }
}

void ChromotingClient::Repaint() {
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingClient::Repaint));
    return;
  }

  view_->Paint();
}

void ChromotingClient::SetViewport(int x, int y, int width, int height) {
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingClient::SetViewport,
                          x, y, width, height));
    return;
  }

  view_->SetViewport(x, y, width, height);
}

void ChromotingClient::ProcessVideoPacket(const VideoPacket* packet,
                                          Task* done) {
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingClient::ProcessVideoPacket,
                          packet, done));
    return;
  }

  received_packets_.push_back(QueuedVideoPacket(packet, done));
  if (!packet_being_processed_)
    DispatchPacket();
}

int ChromotingClient::GetPendingPackets() {
  return received_packets_.size();
}

void ChromotingClient::DispatchPacket() {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  CHECK(!packet_being_processed_);

  if (received_packets_.empty()) {
    // Nothing to do!
    return;
  }

  const VideoPacket* packet = received_packets_.front().packet;
  packet_being_processed_ = true;

  ScopedTracer tracer("Handle video packet");
  rectangle_decoder_->DecodePacket(
      packet, NewTracedMethod(this, &ChromotingClient::OnPacketDone));
}

void ChromotingClient::OnConnectionOpened(protocol::ConnectionToHost* conn) {
  VLOG(1) << "ChromotingClient::OnConnectionOpened";
  Initialize();
  SetConnectionState(CONNECTED);
}

void ChromotingClient::OnConnectionClosed(protocol::ConnectionToHost* conn) {
  VLOG(1) << "ChromotingClient::OnConnectionClosed";
  SetConnectionState(DISCONNECTED);
}

void ChromotingClient::OnConnectionFailed(protocol::ConnectionToHost* conn) {
  VLOG(1) << "ChromotingClient::OnConnectionFailed";
  SetConnectionState(FAILED);
}

MessageLoop* ChromotingClient::message_loop() {
  return context_->jingle_thread()->message_loop();
}

void ChromotingClient::SetConnectionState(ConnectionState s) {
  // TODO(ajwong): We actually may want state to be a shared variable. Think
  // through later.
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingClient::SetConnectionState, s));
    return;
  }

  state_ = s;
  view_->SetConnectionState(s);

  Repaint();
}

void ChromotingClient::OnPacketDone() {
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewTracedMethod(this, &ChromotingClient::OnPacketDone));
    return;
  }

  TraceContext::tracer()->PrintString("Packet done");

  received_packets_.front().done->Run();
  delete received_packets_.front().done;
  received_packets_.pop_front();

  packet_being_processed_ = false;

  DispatchPacket();
}

void ChromotingClient::Initialize() {
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewTracedMethod(this, &ChromotingClient::Initialize));
    return;
  }

  TraceContext::tracer()->PrintString("Initializing client.");

  const protocol::SessionConfig* config = connection_->config();

  // Resize the window.
  int width = config->initial_resolution().width;
  int height = config->initial_resolution().height;
  VLOG(1) << "Initial screen geometry: " << width << "x" << height;

  // TODO(ajwong): What to do here?  Does the decoder actually need to request
  // the right frame size?  This is mainly an optimization right?
  // rectangle_decoder_->SetOutputFrameSize(width, height);
  view_->SetViewport(0, 0, width, height);

  // Initialize the decoder.
  rectangle_decoder_->Initialize(config);

  // Schedule the input handler to process the event queue.
  input_handler_->Initialize();
}

////////////////////////////////////////////////////////////////////////////
// ClientStub control channel interface.
void ChromotingClient::NotifyResolution(
    const protocol::NotifyResolutionRequest* msg, Task* done) {
  NOTIMPLEMENTED();
  done->Run();
  delete done;
}

void ChromotingClient::BeginSessionResponse(
    const protocol::LocalLoginStatus* msg, Task* done) {
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingClient::BeginSessionResponse,
                          msg, done));
    return;
  }

  // Inform the connection that the client has been authenticated. This will
  // enable the communication channels.
  if (msg->success()) {
    connection_->OnClientAuthenticated();
  }

  view_->UpdateLoginStatus(msg->success(), msg->error_info());
  done->Run();
  delete done;
}

}  // namespace remoting
