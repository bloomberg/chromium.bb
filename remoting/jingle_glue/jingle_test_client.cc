// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if !defined(OS_WIN)
extern "C" {
#include <unistd.h>
}
#endif  // !defined(OS_WIN)

#include <iostream>
#include <list>

#include "base/at_exit.h"
#include "media/base/data_buffer.h"
#include "remoting/jingle_glue/jingle_channel.h"
#include "remoting/jingle_glue/jingle_client.h"

using remoting::JingleClient;
using remoting::JingleChannel;

void SetConsoleEcho(bool on) {
#if defined(OS_WIN)
  HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
  if ((hIn == INVALID_HANDLE_VALUE) || (hIn == NULL))
    return;

  DWORD mode;
  if (!GetConsoleMode(hIn, &mode))
    return;

  if (on) {
    mode = mode | ENABLE_ECHO_INPUT;
  } else {
    mode = mode & ~ENABLE_ECHO_INPUT;
  }

  SetConsoleMode(hIn, mode);
#else  // defined(OS_WIN)
  if (on)
    system("stty echo");
  else
    system("stty -echo");
#endif  // !defined(OS_WIN)
}

class JingleTestClient : public JingleChannel::Callback,
                         public JingleClient::Callback {
 public:
  virtual ~JingleTestClient() {}

  void Run(const std::string& username, const std::string& password,
           const std::string& host_jid) {
    client_ = new JingleClient();
    client_->Init(username, password, this);

    if (host_jid != "") {
      scoped_refptr<JingleChannel> channel = client_->Connect(host_jid, this);
      channels_.push_back(channel);
    }

    while (true) {
      std::string line;
      std::getline(std::cin, line);

      {
        AutoLock auto_lock(channels_lock_);

        // Broadcast message to all clients.
        for (ChannelsList::iterator it = channels_.begin();
            it != channels_.end(); ++it) {
          uint8* buf = new uint8[line.length()];
          memcpy(buf, line.c_str(), line.length());
          (*it)->Write(new media::DataBuffer(buf, line.length()));
        }
      }

      if (line == "exit")
        break;
    }

    while (!channels_.empty()) {
      channels_.front()->Close();
      channels_.pop_front();
    }

    client_->Close();
  }

  // JingleChannel::Callback interface.
  void OnStateChange(JingleChannel* channel, JingleChannel::State state) {
    LOG(INFO) << "State of " << channel->jid() << " changed to " << state;
  }

  void OnPacketReceived(JingleChannel* channel,
                        scoped_refptr<media::DataBuffer> buffer) {
    std::string str(reinterpret_cast<const char*>(buffer->GetData()),
                    buffer->GetDataSize());
    std::cout << "(" << channel->jid() << "): " << str << std::endl;
  }

  // JingleClient::Callback interface.
  void OnStateChange(JingleClient* client, JingleClient::State state) {
    if (state == JingleClient::CONNECTED) {
      std::cerr << "Connected as " << client->GetFullJid() << std::endl;
    } else if (state == JingleClient::CLOSED) {
      std::cerr << "Connection closed" << std::endl;
    }
  }

  bool OnAcceptConnection(JingleClient* client, const std::string& jid,
                          JingleChannel::Callback** callback) {
    std::cerr << "Accepting new connection from " << jid << std::endl;
    *callback = this;
    return true;
  }

  void OnNewConnection(JingleClient* client,
                       scoped_refptr<JingleChannel> channel) {
    std::cerr << "Connected to " << channel->jid() << std::endl;
    AutoLock auto_lock(channels_lock_);
    channels_.push_back(channel);
  }

 private:
  typedef std::list<scoped_refptr<JingleChannel> > ChannelsList;

  scoped_refptr<JingleClient> client_;
  ChannelsList channels_;
  Lock channels_lock_;
};

int main(int argc, char** argv) {
  if (argc > 2)
    std::cerr << "Usage: " << argv[0] << " [<host_jid>]" << std::endl;

  base::AtExitManager exit_manager;

  std::string host_jid = argc == 2 ? argv[1] : "";

  std::string username;
  std::cout << "JID: ";
  std::cin >> username;

  std::string password;
  SetConsoleEcho(false);
  std::cout << "Password: ";
  std::cin >> password;
  SetConsoleEcho(true);
  std::cout << std::endl;

  JingleTestClient client;

  client.Run(username, password, host_jid);

  return 0;
}
