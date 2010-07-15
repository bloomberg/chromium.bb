// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple client implements a minimal Chromoting client and shows
// network traffic for debugging.

#include <iostream>
#include <list>

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"

#include "base/waitable_event.h"
#include "media/base/data_buffer.h"
#include "remoting/base/protocol_decoder.h"
#include "remoting/client/client_util.h"
#include "remoting/client/jingle_host_connection.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "remoting/jingle_glue/jingle_thread.h"

using remoting::BeginUpdateStreamMessage;
using remoting::EndUpdateStreamMessage;
using remoting::HostConnection;
using remoting::HostMessage;
using remoting::HostMessageList;
using remoting::InitClientMessage;
using remoting::JingleClient;
using remoting::JingleHostConnection;
using remoting::JingleThread;
using remoting::ProtocolDecoder;
using remoting::UpdateStreamPacketMessage;

class SimpleHostEventCallback : public HostConnection::HostEventCallback {
 public:
  explicit SimpleHostEventCallback(MessageLoop* loop,
                                   base::WaitableEvent* client_done)
      : main_loop_(loop), client_done_(client_done) {
  }

  virtual void HandleMessages(HostConnection* conn,
                              HostMessageList* messages) {
    HostMessageList list;
    messages->swap(list);
    for (size_t i = 0; i < list.size(); ++i) {
      HostMessage* msg = list[i];
      if (msg->has_init_client()) {
        HandleInitClientMessage(msg);
      } else if (msg->has_begin_update_stream()) {
        HandleBeginUpdateStreamMessage(msg);
      } else if (msg->has_update_stream_packet()) {
        HandleUpdateStreamPacketMessage(msg);
      } else if (msg->has_end_update_stream()) {
        HandleEndUpdateStreamMessage(msg);
      }
    }
    STLDeleteElements<HostMessageList>(&list);
  }

  virtual void OnConnectionOpened(HostConnection* conn) {
    std::cout << "Connection establised." << std::endl;
  }

  virtual void OnConnectionClosed(HostConnection* conn) {
    std::cout << "Connection closed." << std::endl;
    client_done_->Signal();
  }

  virtual void OnConnectionFailed(HostConnection* conn) {
    std::cout << "Conection failed." << std::endl;
    client_done_->Signal();
  }

 private:
  void HandleInitClientMessage(HostMessage* host_msg) {
    const InitClientMessage& msg = host_msg->init_client();
    std::cout << "InitClient (" << msg.width()
              << ", " << msg.height() << ")" << std::endl;
  }

  void HandleBeginUpdateStreamMessage(HostMessage* host_msg) {
    const BeginUpdateStreamMessage& msg = host_msg->begin_update_stream();
    std::cout << msg.GetTypeName() << std::endl;
  }

  void HandleUpdateStreamPacketMessage(HostMessage* host_msg) {
    const UpdateStreamPacketMessage& msg = host_msg->update_stream_packet();
    if (!msg.has_begin_rect())
      return;

    std::cout << "UpdateStreamPacket (" << msg.begin_rect().x()
              << ", " << msg.begin_rect().y() << ") ["
              << msg.begin_rect().width() << " x "
              << msg.begin_rect().height() << "]"
              << std::endl;
  }

  void HandleEndUpdateStreamMessage(HostMessage* host_msg) {
    const EndUpdateStreamMessage& msg = host_msg->end_update_stream();
    std::cout << msg.GetTypeName() << std::endl;
  }

  // JingleClient::Callback interface.
  void OnStateChange(JingleClient* client, JingleClient::State state) {
    if (state == JingleClient::CONNECTED) {
      LOG(INFO) << "Connected as: " << client->GetFullJid();
    } else if (state == JingleClient::CLOSED) {
      LOG(INFO) << "Connection closed.";
    }
  }

  MessageLoop* main_loop_;
  base::WaitableEvent* client_done_;

  DISALLOW_COPY_AND_ASSIGN(SimpleHostEventCallback);
};

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  std::string host_jid;
  std::string username;
  std::string auth_token;

  if (!remoting::GetLoginInfo(argc, argv, &host_jid, &username, &auth_token)) {
    std::cerr << "Cannot get valid login info." << std::endl;
    return 1;
  }

  // Start up a thread to run everything.
  JingleThread network_thread;
  network_thread.Start();

  base::WaitableEvent client_done(false, false);
  SimpleHostEventCallback handler(network_thread.message_loop(), &client_done);
  JingleHostConnection connection(&network_thread);
  connection.Connect(username, auth_token, host_jid, &handler);

  // Wait until the mainloop has been signaled to exit.
  client_done.Wait();

  connection.Disconnect();
  network_thread.message_loop()->PostTask(FROM_HERE,
                                          new MessageLoop::QuitTask());
  network_thread.Stop();

  return 0;
}
