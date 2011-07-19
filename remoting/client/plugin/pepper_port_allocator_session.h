// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_PORT_ALLOCATOR_SESSION_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_PORT_ALLOCATOR_SESSION_H_

#include "base/message_loop.h"
#include "ppapi/cpp/completion_callback.h"
#include "third_party/libjingle/source/talk/p2p/client/basicportallocator.h"

namespace remoting {

class ChromotingInstance;
class PepperCreateSessionTask;
class PepperURLFetcher;
class PortAllocatorSessionFactory;

class PepperPortAllocatorSession : public cricket::BasicPortAllocatorSession {
 public:
  PepperPortAllocatorSession(
      ChromotingInstance* instance,
      MessageLoop* message_loop,
      cricket::BasicPortAllocator* allocator,
      const std::string& name,
      const std::string& session_type,
      const std::vector<talk_base::SocketAddress>& stun_hosts,
      const std::vector<std::string>& relay_hosts,
      const std::string& relay,
      const std::string& agent);
  virtual ~PepperPortAllocatorSession();

  const std::string& relay_token() const {
    return relay_token_;
  }

  // Overrides the implementations in BasicPortAllocatorSession to use
  // pepper's URLLoader as HTTP client.
  virtual void SendSessionRequest(const std::string& host, int port);
  virtual void ReceiveSessionResponse(const std::string& response);

  void OnRequestDone(bool success, int status_code,
                     const std::string& response);

 private:
  virtual void GetPortConfigurations();
  void TryCreateRelaySession();

  ChromotingInstance* const instance_;
  MessageLoop* const jingle_message_loop_;

  std::vector<std::string> relay_hosts_;
  std::vector<talk_base::SocketAddress> stun_hosts_;
  std::string relay_token_;
  std::string agent_;
  int attempts_;

  scoped_refptr<PepperCreateSessionTask> create_session_task_;

  DISALLOW_COPY_AND_ASSIGN(PepperPortAllocatorSession);
};

PortAllocatorSessionFactory* CreatePepperPortAllocatorSessionFactory(
    ChromotingInstance* instance);

}  // namespace remoting

#endif // REMOTING_CLIENT_PLUGIN_PEPPER_PORT_ALLOCATOR_SESSION_H_
