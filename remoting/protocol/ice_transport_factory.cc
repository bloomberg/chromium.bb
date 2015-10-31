// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/ice_transport_factory.h"

#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "remoting/protocol/ice_transport.h"
#include "third_party/webrtc/p2p/client/httpportallocator.h"

namespace remoting {
namespace protocol {

// Get fresh STUN/Relay configuration every hour.
static const int kJingleInfoUpdatePeriodSeconds = 3600;

IceTransportFactory::IceTransportFactory(
    SignalStrategy* signal_strategy,
    scoped_ptr<cricket::HttpPortAllocatorBase> port_allocator,
    const NetworkSettings& network_settings,
    TransportRole role)
    : signal_strategy_(signal_strategy),
      port_allocator_(port_allocator.Pass()),
      network_settings_(network_settings),
      role_(role) {}

IceTransportFactory::~IceTransportFactory() {
  // This method may be called in response to a libjingle signal, so
  // libjingle objects must be deleted asynchronously.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
  task_runner->DeleteSoon(FROM_HERE, port_allocator_.release());
}

scoped_ptr<Transport> IceTransportFactory::CreateTransport() {
  scoped_ptr<IceTransport> result(
      new IceTransport(port_allocator_.get(), network_settings_, role_));

  EnsureFreshJingleInfo();

  // If there is a pending |jingle_info_request_| delay starting the new
  // transport until the request is finished.
  if (jingle_info_request_) {
    on_jingle_info_callbacks_.push_back(result->GetCanStartClosure());
  } else {
    result->GetCanStartClosure().Run();
  }

  return result.Pass();
}

void IceTransportFactory::EnsureFreshJingleInfo() {
  uint32 stun_or_relay_flags = NetworkSettings::NAT_TRAVERSAL_STUN |
      NetworkSettings::NAT_TRAVERSAL_RELAY;
  if (!(network_settings_.flags & stun_or_relay_flags) ||
      jingle_info_request_) {
    return;
  }

  if (last_jingle_info_update_time_.is_null() ||
      base::TimeTicks::Now() - last_jingle_info_update_time_ >
          base::TimeDelta::FromSeconds(kJingleInfoUpdatePeriodSeconds)) {
    jingle_info_request_.reset(new JingleInfoRequest(signal_strategy_));
    jingle_info_request_->Send(base::Bind(
        &IceTransportFactory::OnJingleInfo, base::Unretained(this)));
  }
}

void IceTransportFactory::OnJingleInfo(
    const std::string& relay_token,
    const std::vector<std::string>& relay_hosts,
    const std::vector<rtc::SocketAddress>& stun_hosts) {
  if (!relay_token.empty() && !relay_hosts.empty()) {
    port_allocator_->SetRelayHosts(relay_hosts);
    port_allocator_->SetRelayToken(relay_token);
  }
  if (!stun_hosts.empty()) {
    port_allocator_->SetStunHosts(stun_hosts);
  }

  jingle_info_request_.reset();
  if ((!relay_token.empty() && !relay_hosts.empty()) || !stun_hosts.empty())
    last_jingle_info_update_time_ = base::TimeTicks::Now();

  while (!on_jingle_info_callbacks_.empty()) {
    on_jingle_info_callbacks_.begin()->Run();
    on_jingle_info_callbacks_.pop_front();
  }
}

}  // namespace protocol
}  // namespace remoting
