// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_client.h"

#include "base/message_loop.h"
#include "remoting/base/tracer.h"
#include "remoting/client/chromoting_view.h"
#include "remoting/client/client_context.h"
#include "remoting/client/host_connection.h"
#include "remoting/client/input_handler.h"
#include "remoting/client/rectangle_update_decoder.h"

namespace remoting {

ChromotingClient::ChromotingClient(const ClientConfig& config,
                                   ClientContext* context,
                                   HostConnection* connection,
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
      message_being_processed_(false) {
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

  connection_->Connect(config_, this);

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

void ChromotingClient::HandleMessage(HostConnection* conn,
                                     ChromotingHostMessage* msg) {
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingClient::HandleMessage,
                          conn, msg));
    return;
  }

  // Put all messages in the queue.
  received_messages_.push_back(msg);

  if (!message_being_processed_) {
    DispatchMessage();
  }
}

void ChromotingClient::DispatchMessage() {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  CHECK(!message_being_processed_);

  if (received_messages_.empty()) {
    // Nothing to do!
    return;
  }

  ChromotingHostMessage* msg = received_messages_.front();
  received_messages_.pop_front();
  message_being_processed_ = true;

  // TODO(ajwong): Consider creating a macro similar to the IPC message
  // mappings.  Also reconsider the lifetime of the message object.
  if (msg->has_init_client()) {
    ScopedTracer tracer("Handle Init Client");
    // TODO(ajwong): Change this to use a done callback.
    InitClient(msg->init_client(),
               NewTracedMethod(this, &ChromotingClient::OnMessageDone, msg));
  } else if (msg->has_rectangle_update()) {
    ScopedTracer tracer("Handle Rectangle Update");
    rectangle_decoder_->DecodePacket(
        msg->rectangle_update(),
        NewTracedMethod(this, &ChromotingClient::OnMessageDone, msg));
  } else {
    NOTREACHED() << "Unknown message received";

    // We have an unknown message. Drop it, and schedule another dispatch.
    // Call DispatchMessage as a continuation to avoid growing the stack.
    delete msg;
    message_being_processed_ = false;
    message_loop()->PostTask(
        FROM_HERE,
        NewTracedMethod(this, &ChromotingClient::DispatchMessage));
    return;
  }
}

void ChromotingClient::OnConnectionOpened(HostConnection* conn) {
  LOG(INFO) << "ChromotingClient::OnConnectionOpened";
  SetConnectionState(CONNECTED);
}

void ChromotingClient::OnConnectionClosed(HostConnection* conn) {
  LOG(INFO) << "ChromotingClient::OnConnectionClosed";
  SetConnectionState(DISCONNECTED);
}

void ChromotingClient::OnConnectionFailed(HostConnection* conn) {
  LOG(INFO) << "ChromotingClient::OnConnectionFailed";
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

void ChromotingClient::OnMessageDone(ChromotingHostMessage* message) {
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewTracedMethod(this, &ChromotingClient::OnMessageDone, message));
    return;
  }

  TraceContext::tracer()->PrintString("Message done");

  message_being_processed_ = false;
  delete message;
  DispatchMessage();
}

void ChromotingClient::InitClient(const InitClientMessage& init_client,
                                  Task* done) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  TraceContext::tracer()->PrintString("Init received");

  // Resize the window.
  int width = init_client.width();
  int height = init_client.height();
  LOG(INFO) << "Init client received geometry: " << width << "x" << height;

//  TODO(ajwong): What to do here?  Does the decoder actually need to request
//  the right frame size?  This is mainly an optimization right?
//  rectangle_decoder_->SetOutputFrameSize(width, height);
  view_->SetViewport(0, 0, width, height);

  // Schedule the input handler to process the event queue.
  input_handler_->Initialize();

  done->Run();
  delete done;
}

}  // namespace remoting
