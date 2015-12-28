// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/transport_context.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "remoting/protocol/port_allocator_factory.h"
#include "third_party/webrtc/p2p/client/httpportallocator.h"

#if !defined(OS_NACL)
#include "jingle/glue/thread_wrapper.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/protocol/chromium_port_allocator.h"
#endif  // !defined(OS_NACL)

namespace remoting {
namespace protocol {

// Get fresh STUN/Relay configuration every hour.
static const int kJingleInfoUpdatePeriodSeconds = 3600;

#if !defined(OS_NACL)
// static
scoped_refptr<TransportContext> TransportContext::ForTests(TransportRole role) {
  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();
  return new protocol::TransportContext(
             nullptr, make_scoped_ptr(
                          new protocol::ChromiumPortAllocatorFactory(nullptr)),
             protocol::NetworkSettings(
                 protocol::NetworkSettings::NAT_TRAVERSAL_OUTGOING),
             role);
}
#endif  // !defined(OS_NACL)

TransportContext::TransportContext(
    SignalStrategy* signal_strategy,
    scoped_ptr<PortAllocatorFactory> port_allocator_factory,
    const NetworkSettings& network_settings,
    TransportRole role)
    : signal_strategy_(signal_strategy),
      port_allocator_factory_(std::move(port_allocator_factory)),
      network_settings_(network_settings),
      role_(role) {}

TransportContext::~TransportContext() {}

void TransportContext::Prepare() {
  EnsureFreshJingleInfo();
}

void TransportContext::CreatePortAllocator(
    const CreatePortAllocatorCallback& callback) {
  EnsureFreshJingleInfo();

  // If there is a pending |jingle_info_request_| delay starting the new
  // transport until the request is finished.
  if (jingle_info_request_) {
    pending_port_allocator_requests_.push_back(callback);
  } else {
    callback.Run(CreatePortAllocatorInternal());
  }
}

void TransportContext::EnsureFreshJingleInfo() {
  // Check if request is already pending.
  if (jingle_info_request_)
    return;

  // Don't need to make jingleinfo request if both STUN and Relay are disabled.
  if ((network_settings_.flags & (NetworkSettings::NAT_TRAVERSAL_STUN |
                                  NetworkSettings::NAT_TRAVERSAL_RELAY)) == 0) {
    return;
  }

  if (last_jingle_info_update_time_.is_null() ||
      base::TimeTicks::Now() - last_jingle_info_update_time_ >
          base::TimeDelta::FromSeconds(kJingleInfoUpdatePeriodSeconds)) {
    jingle_info_request_.reset(new JingleInfoRequest(signal_strategy_));
    jingle_info_request_->Send(base::Bind(
        &TransportContext::OnJingleInfo, base::Unretained(this)));
  }
}

void TransportContext::OnJingleInfo(
    const std::string& relay_token,
    const std::vector<std::string>& relay_hosts,
    const std::vector<rtc::SocketAddress>& stun_hosts) {
  relay_token_ = relay_token;
  relay_hosts_ = relay_hosts;
  stun_hosts_ = stun_hosts;

  jingle_info_request_.reset();
  if ((!relay_token.empty() && !relay_hosts.empty()) || !stun_hosts.empty())
    last_jingle_info_update_time_ = base::TimeTicks::Now();

  while (!pending_port_allocator_requests_.empty()) {
    pending_port_allocator_requests_.begin()->Run(
        CreatePortAllocatorInternal());
    pending_port_allocator_requests_.pop_front();
  }
}

scoped_ptr<cricket::PortAllocator>
TransportContext::CreatePortAllocatorInternal() {
  scoped_ptr<cricket::HttpPortAllocatorBase> result =
      port_allocator_factory_->CreatePortAllocator();

  if (!relay_token_.empty() && !relay_hosts_.empty()) {
    result->SetRelayHosts(relay_hosts_);
    result->SetRelayToken(relay_token_);
  }
  if (!stun_hosts_.empty())
    result->SetStunHosts(stun_hosts_);

  // We always use PseudoTcp to provide a reliable channel. It provides poor
  // performance when combined with TCP-based transport, so we have to disable
  // TCP ports. ENABLE_SHARED_UFRAG flag is specified so that the same username
  // fragment is shared between all candidates.
  int flags = cricket::PORTALLOCATOR_DISABLE_TCP |
              cricket::PORTALLOCATOR_ENABLE_SHARED_UFRAG |
              cricket::PORTALLOCATOR_ENABLE_IPV6;

 if (!(network_settings_.flags & NetworkSettings::NAT_TRAVERSAL_STUN))
    flags |= cricket::PORTALLOCATOR_DISABLE_STUN;

  if (!(network_settings_.flags & NetworkSettings::NAT_TRAVERSAL_RELAY))
    flags |= cricket::PORTALLOCATOR_DISABLE_RELAY;

  result->set_flags(flags);
  result->SetPortRange(network_settings_.port_range.min_port,
                       network_settings_.port_range.max_port);

  return std::move(result);
}

}  // namespace protocol
}  // namespace remoting
