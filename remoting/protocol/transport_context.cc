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
#include "remoting/protocol/jingle_info_request.h"
#include "remoting/protocol/port_allocator_factory.h"
#include "third_party/webrtc/base/socketaddress.h"

#if !defined(OS_NACL)
#include "jingle/glue/thread_wrapper.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#endif  // !defined(OS_NACL)

namespace remoting {
namespace protocol {

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

void TransportContext::GetIceConfig(const GetIceConfigCallback& callback) {
  EnsureFreshJingleInfo();

  // If there is a pending |ice_config_request_| delay the callback until the
  // request is finished.
  if (ice_config_request_) {
    pending_ice_config_callbacks_.push_back(callback);
  } else {
    callback.Run(ice_config_);
  }
}

void TransportContext::EnsureFreshJingleInfo() {
  // Check if request is already pending.
  if (ice_config_request_)
    return;

  // Don't need to make jingleinfo request if both STUN and Relay are disabled.
  if ((network_settings_.flags & (NetworkSettings::NAT_TRAVERSAL_STUN |
                                  NetworkSettings::NAT_TRAVERSAL_RELAY)) == 0) {
    return;
  }

  if (ice_config_.is_null() ||
      base::Time::Now() > ice_config_.expiration_time) {
    ice_config_request_.reset(new JingleInfoRequest(signal_strategy_));
    ice_config_request_->Send(base::Bind(
        &TransportContext::OnIceConfig, base::Unretained(this)));
  }
}

void TransportContext::OnIceConfig(const IceConfig& ice_config) {
  ice_config_ = ice_config;
  ice_config_request_.reset();

  while (!pending_ice_config_callbacks_.empty()) {
    pending_ice_config_callbacks_.begin()->Run(ice_config_);
    pending_ice_config_callbacks_.pop_front();
  }
}

}  // namespace protocol
}  // namespace remoting
