// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_client.h"

#include "base/message_loop.h"
#include "remoting/client/chromoting_view.h"
#include "remoting/client/client_config.h"
#include "remoting/client/client_context.h"
#include "remoting/client/host_connection.h"
#include "remoting/client/input_handler.h"

static const uint32 kCreatedColor = 0xffccccff;
static const uint32 kDisconnectedColor = 0xff00ccff;
static const uint32 kFailedColor = 0xffcc00ff;

namespace remoting {

ChromotingClient::ChromotingClient(ClientConfig* config,
                                   ClientContext* context,
                                   HostConnection* connection,
                                   ChromotingView* view,
                                   InputHandler* input_handler,
                                   CancelableTask* client_done)
    : config_(config),
      context_(context),
      connection_(connection),
      view_(view),
      input_handler_(input_handler),
      client_done_(client_done),
      state_(CREATED) {
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

  // Quit the current message loop.
  message_loop()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
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

void ChromotingClient::HandleMessages(HostConnection* conn,
                                      HostMessageList* messages) {
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingClient::HandleMessages,
                          conn, messages));
    return;
  }

  for (size_t i = 0; i < messages->size(); ++i) {
    HostMessage* msg = (*messages)[i];
    // TODO(ajwong): Consider creating a macro similar to the IPC message
    // mappings.  Also reconsider the lifetime of the message object.
    if (msg->has_init_client()) {
      InitClient(msg);
    } else if (msg->has_begin_update_stream()) {
      BeginUpdate(msg);
    } else if (msg->has_update_stream_packet()) {
      HandleUpdate(msg);
    } else if (msg->has_end_update_stream()) {
      EndUpdate(msg);
    } else {
      NOTREACHED() << "Unknown message received";
    }
  }
  // Assume we have processed all the messages.
  messages->clear();
}

void ChromotingClient::OnConnectionOpened(HostConnection* conn) {
  LOG(INFO) << "ChromotingClient::OnConnectionOpened";
  SetState(CONNECTED);
}

void ChromotingClient::OnConnectionClosed(HostConnection* conn) {
  LOG(INFO) << "ChromotingClient::OnConnectionClosed";
  SetState(DISCONNECTED);
}

void ChromotingClient::OnConnectionFailed(HostConnection* conn) {
  LOG(INFO) << "ChromotingClient::OnConnectionFailed";
  SetState(FAILED);
}

MessageLoop* ChromotingClient::message_loop() {
  return context_->jingle_thread()->message_loop();
}

void ChromotingClient::SetState(State s) {
  // TODO(ajwong): We actually may want state to be a shared variable. Think
  // through later.
  if (message_loop() != MessageLoop::current()) {
    message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &ChromotingClient::SetState, s));
    return;
  }

  state_ = s;
  switch (state_) {
    case CREATED:
      view_->SetSolidFill(kCreatedColor);
      break;

    case CONNECTED:
      view_->UnsetSolidFill();
      break;

    case DISCONNECTED:
      view_->SetSolidFill(kDisconnectedColor);
      break;

    case FAILED:
      view_->SetSolidFill(kFailedColor);
      break;
  }

  Repaint();
}

void ChromotingClient::InitClient(HostMessage* msg) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(msg->has_init_client());
  scoped_ptr<HostMessage> deleter(msg);

  // Resize the window.
  int width = msg->init_client().width();
  int height = msg->init_client().height();
  LOG(INFO) << "Init client received geometry: " << width << "x" << height;

  view_->SetHostScreenSize(width, height);

  // Schedule the input handler to process the event queue.
  input_handler_->Initialize();
}

void ChromotingClient::BeginUpdate(HostMessage* msg) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(msg->has_begin_update_stream());

  view_->HandleBeginUpdateStream(msg);
}

void ChromotingClient::HandleUpdate(HostMessage* msg) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(msg->has_update_stream_packet());

  view_->HandleUpdateStreamPacket(msg);
}

void ChromotingClient::EndUpdate(HostMessage* msg) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(msg->has_end_update_stream());

  view_->HandleEndUpdateStream(msg);
}

}  // namespace remoting
