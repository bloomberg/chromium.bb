// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_client.h"

#include "base/message_loop.h"
#include "remoting/client/chromoting_view.h"
#include "remoting/client/host_connection.h"

static const uint32 kCreatedColor = 0xffccccff;
static const uint32 kDisconnectedColor = 0xff00ccff;
static const uint32 kFailedColor = 0xffcc00ff;

namespace remoting {

ChromotingClient::ChromotingClient(MessageLoop* message_loop,
                                   HostConnection* connection,
                                   ChromotingView* view)
    : message_loop_(message_loop),
      state_(CREATED),
      host_connection_(connection),
      view_(view) {
}

ChromotingClient::~ChromotingClient() {
}

void ChromotingClient::Repaint() {
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ChromotingClient::DoRepaint));
}

void ChromotingClient::SetViewport(int x, int y, int width, int height) {
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ChromotingClient::DoSetViewport,
                        x, y, width, height));
}

void ChromotingClient::HandleMessages(HostConnection* conn,
                                      HostMessageList* messages) {
  for (size_t i = 0; i < messages->size(); ++i) {
    HostMessage* msg = (*messages)[i];
    // TODO(ajwong): Consider creating a macro similar to the IPC message
    // mappings.  Also reconsider the lifetime of the message object.
    if (msg->has_init_client()) {
      message_loop()->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &ChromotingClient::DoInitClient, msg));
    } else if (msg->has_begin_update_stream()) {
      message_loop()->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &ChromotingClient::DoBeginUpdate, msg));
    } else if (msg->has_update_stream_packet()) {
      message_loop()->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &ChromotingClient::DoHandleUpdate, msg));
    } else if (msg->has_end_update_stream()) {
      message_loop()->PostTask(
          FROM_HERE,
          NewRunnableMethod(this, &ChromotingClient::DoEndUpdate, msg));
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
  return message_loop_;
}

void ChromotingClient::SetState(State s) {
  // TODO(ajwong): We actually may want state to be a shared variable. Think
  // through later.
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ChromotingClient::DoSetState, s));
}

void ChromotingClient::DoSetState(State s) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

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

  DoRepaint();
}

void ChromotingClient::DoRepaint() {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  view_->Paint();
}

void ChromotingClient::DoSetViewport(int x, int y, int width, int height) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  view_->SetViewport(x, y, width, height);
}

void ChromotingClient::DoInitClient(HostMessage* msg) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(msg->has_init_client());
  scoped_ptr<HostMessage> deleter(msg);

  // Saves the dimension and resize the window.
  int width = msg->init_client().width();
  int height = msg->init_client().height();
  LOG(INFO) << "Init client received geometry: " << width << "x" << height;

  view_->SetBackingStoreSize(width, height);
}

void ChromotingClient::DoBeginUpdate(HostMessage* msg) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(msg->has_begin_update_stream());

  view_->HandleBeginUpdateStream(msg);
}

void ChromotingClient::DoHandleUpdate(HostMessage* msg) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(msg->has_update_stream_packet());

  view_->HandleUpdateStreamPacket(msg);
}

void ChromotingClient::DoEndUpdate(HostMessage* msg) {
  DCHECK_EQ(message_loop(), MessageLoop::current());
  DCHECK(msg->has_end_update_stream());

  view_->HandleEndUpdateStream(msg);
}

}  // namespace remoting
