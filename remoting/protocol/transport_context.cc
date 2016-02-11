// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/transport_context.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "remoting/base/url_request.h"
#include "remoting/protocol/port_allocator_factory.h"
#include "third_party/webrtc/base/socketaddress.h"

#if !defined(OS_NACL)
#include "jingle/glue/thread_wrapper.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
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
      nullptr, make_scoped_ptr(new protocol::ChromiumPortAllocatorFactory()),
      nullptr, protocol::NetworkSettings(
                   protocol::NetworkSettings::NAT_TRAVERSAL_OUTGOING),
      role);
}
#endif  // !defined(OS_NACL)

TransportContext::TransportContext(
    SignalStrategy* signal_strategy,
    scoped_ptr<PortAllocatorFactory> port_allocator_factory,
    scoped_ptr<UrlRequestFactory> url_request_factory,
    const NetworkSettings& network_settings,
    TransportRole role)
    : signal_strategy_(signal_strategy),
      port_allocator_factory_(std::move(port_allocator_factory)),
      url_request_factory_(std::move(url_request_factory)),
      network_settings_(network_settings),
      role_(role) {}

TransportContext::~TransportContext() {}

void TransportContext::Prepare() {
  EnsureFreshJingleInfo();
}

void TransportContext::GetJingleInfo(const GetJingleInfoCallback& callback) {
  EnsureFreshJingleInfo();

  // If there is a pending |jingle_info_request_| delay the callback until the
  // request is finished.
  if (jingle_info_request_) {
    pending_jingle_info_callbacks_.push_back(callback);
  } else {
    callback.Run(stun_hosts_, relay_hosts_, relay_token_);
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

  while (!pending_jingle_info_callbacks_.empty()) {
    pending_jingle_info_callbacks_.begin()->Run(stun_hosts_, relay_hosts_,
                                                relay_token_);
    pending_jingle_info_callbacks_.pop_front();
  }
}

}  // namespace protocol
}  // namespace remoting
