// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/relay_port_allocator.h"

#include <vector>

#include "remoting/jingle_glue/jingle_info_task.h"
#include "third_party/libjingle/source/talk/xmpp/xmppclient.h"

namespace remoting {

RelayPortAllocator::RelayPortAllocator(
    talk_base::NetworkManager* network_manager,
    const std::string& user_agent)
    : cricket::HttpPortAllocator(network_manager, user_agent) {
}

RelayPortAllocator::~RelayPortAllocator() {}

void RelayPortAllocator::OnJingleInfo(
    const std::string& token,
    const std::vector<std::string>& relay_hosts,
    const std::vector<talk_base::SocketAddress>& stun_hosts) {
  this->SetRelayToken(token);
  this->SetStunHosts(stun_hosts);
  this->SetRelayHosts(relay_hosts);
}

void RelayPortAllocator::SetJingleInfo(buzz::XmppClient* client) {
  // The JingleInfoTask is freed by the task-runner.
  JingleInfoTask* jit = new JingleInfoTask(client);
  jit->SignalJingleInfo.connect(this, &RelayPortAllocator::OnJingleInfo);
  jit->Start();
  jit->RefreshJingleInfoNow();
}

}  // namespace remoting
