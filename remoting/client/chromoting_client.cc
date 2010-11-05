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
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/connection_to_host.h"

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
                       this, this);

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

void ChromotingClient::HandleMessage(protocol::ConnectionToHost* conn,
                                     ChromotingHostMessage* msg) {
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingClient::HandleMessage,
                          conn, msg));
    return;
  }

  // TODO(ajwong): Consider creating a macro similar to the IPC message
  // mappings.  Also reconsider the lifetime of the message object.
  if (msg->has_init_client()) {
    ScopedTracer tracer("Handle Init Client");
    InitClient(msg->init_client());
    delete msg;
  } else {
    NOTREACHED() << "Unknown message received";
  }
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
      *packet, NewTracedMethod(this, &ChromotingClient::OnPacketDone));
}

void ChromotingClient::OnConnectionOpened(protocol::ConnectionToHost* conn) {
  VLOG(1) << "ChromotingClient::OnConnectionOpened";
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

void ChromotingClient::InitClient(const InitClientMessage& init_client) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  TraceContext::tracer()->PrintString("Init received");

  // Resize the window.
  int width = init_client.width();
  int height = init_client.height();
  VLOG(1) << "Init client received geometry: " << width << "x" << height;

//  TODO(ajwong): What to do here?  Does the decoder actually need to request
//  the right frame size?  This is mainly an optimization right?
//  rectangle_decoder_->SetOutputFrameSize(width, height);
  view_->SetViewport(0, 0, width, height);

  // Schedule the input handler to process the event queue.
  input_handler_->Initialize();
}

}  // namespace remoting
