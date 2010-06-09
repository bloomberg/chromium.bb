// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple client implements a minimalize Chromoting client and shows
// network traffic for debugging.

#include <iostream>
#include <list>

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "remoting/base/protocol_decoder.h"
#include "remoting/client/client_util.h"
#include "remoting/client/host_connection.h"

using remoting::BeginUpdateStreamMessage;
using remoting::EndUpdateStreamMessage;
using remoting::HostConnection;
using remoting::HostMessage;
using remoting::HostMessageList;
using remoting::InitClientMessage;
using remoting::JingleClient;
using remoting::JingleChannel;
using remoting::ProtocolDecoder;
using remoting::UpdateStreamPacketMessage;

class SimpleHostEventHandler : public HostConnection::EventHandler {
 public:
  SimpleHostEventHandler(MessageLoop* loop)
      : main_loop_(loop) {
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

    // Quit the message if the connection has closed.
    DCHECK(main_loop_);
    main_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  virtual void OnConnectionFailed(HostConnection* conn) {
    std::cout << "Conection failed." << std::endl;

    // Quit the message if the connection has failed.
    DCHECK(main_loop_);
    main_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

 private:
  void HandleInitClientMessage(HostMessage* host_msg) {
    const InitClientMessage& msg = host_msg->init_client();
    std::cout << "InitClient (" << msg.width()
              << ", " << msg.height() << ")" << std::endl;
  }

  void HandleBeginUpdateStreamMessage(HostMessage* host_msg) {
    const BeginUpdateStreamMessage& msg = host_msg->begin_update_stream();
  }

  void HandleUpdateStreamPacketMessage(HostMessage* host_msg) {
    const UpdateStreamPacketMessage& msg = host_msg->update_stream_packet();
    std::cout << "UpdateStreamPacket (" << msg.header().x()
              << ", " << msg.header().y() << ") ["
              << msg.header().width() << " x " << msg.header().height() << "]"
              << std::endl;
  }

  void HandleEndUpdateStreamMessage(HostMessage* host_msg) {
    const EndUpdateStreamMessage& msg = host_msg->end_update_stream();
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

  DISALLOW_COPY_AND_ASSIGN(SimpleHostEventHandler);
};

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  std::string host_jid, username, auth_token;

  if (remoting::GetLoginInfo(host_jid, username, auth_token)) {
    std::cerr << "Cannot get valid login info." << std::endl;
    return 1;
  }

  // The message loop that everything runs on.
  MessageLoop main_loop;
  SimpleHostEventHandler handler(&main_loop);
  HostConnection connection(new ProtocolDecoder(), &handler);
  connection.Connect(username, auth_token, host_jid);

  // Run the message.
  main_loop.Run();
  return 0;
}
