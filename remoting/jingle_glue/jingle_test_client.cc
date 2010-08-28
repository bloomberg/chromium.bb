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
#include "base/nss_util.h"
#include "base/time.h"
#include "media/base/data_buffer.h"
#include "remoting/base/constants.h"
#include "remoting/jingle_glue/jingle_channel.h"
#include "remoting/jingle_glue/jingle_client.h"
#include "remoting/jingle_glue/jingle_thread.h"

using remoting::JingleClient;
using remoting::JingleChannel;
using remoting::kChromotingTokenServiceName;

class JingleTestClient : public JingleChannel::Callback,
                         public JingleClient::Callback,
                         public base::RefCountedThreadSafe<JingleTestClient> {
 public:
  JingleTestClient()
      : closed_event_(true, false) {
  }

  virtual ~JingleTestClient() {}

  void Run(const std::string& username, const std::string& auth_token,
           const std::string& host_jid) {
    remoting::JingleThread jingle_thread;
    jingle_thread.Start();
    client_ = new JingleClient(&jingle_thread);
    client_->Init(username, auth_token, kChromotingTokenServiceName, this);

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
      closed_event_.Reset();
      channels_.front()->Close(
          NewRunnableMethod(this, &JingleTestClient::OnClosed));
      // Wait until channel is closed. If it is not closed within 0.1 seconds
      // continue closing everything else.
      closed_event_.TimedWait(base::TimeDelta::FromMilliseconds(100));
      channels_.pop_front();
    }

    client_->Close();
    jingle_thread.Stop();
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

  void OnClosed() {
    closed_event_.Signal();
  }

 private:
  typedef std::list<scoped_refptr<JingleChannel> > ChannelsList;

  scoped_refptr<JingleClient> client_;
  ChannelsList channels_;
  Lock channels_lock_;
  base::WaitableEvent closed_event_;
};

int main(int argc, char** argv) {
  if (argc > 2)
    std::cerr << "Usage: " << argv[0] << " [<host_jid>]" << std::endl;

  base::AtExitManager exit_manager;

  base::EnsureNSPRInit();
  base::EnsureNSSInit();

  std::string host_jid = argc == 2 ? argv[1] : "";

  std::string username;
  std::cout << "JID: ";
  std::cin >> username;

  std::string auth_token;
  std::cout << "Auth token: ";
  std::cin >> auth_token;

  scoped_refptr<JingleTestClient> client = new JingleTestClient();

  client->Run(username, auth_token, host_jid);

  return 0;
}
