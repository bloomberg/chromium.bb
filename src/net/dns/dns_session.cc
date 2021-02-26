// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_session.h"

#include <stdint.h>

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_socket_allocator.h"
#include "net/log/net_log.h"

namespace net {

DnsSession::DnsSession(const DnsConfig& config,
                       std::unique_ptr<DnsSocketAllocator> socket_allocator,
                       const RandIntCallback& rand_int_callback,
                       NetLog* net_log)
    : config_(config),
      socket_allocator_(std::move(socket_allocator)),
      rand_callback_(base::BindRepeating(rand_int_callback,
                                         0,
                                         std::numeric_limits<uint16_t>::max())),
      net_log_(net_log) {
  UMA_HISTOGRAM_CUSTOM_COUNTS("AsyncDNS.ServerCount",
                              config_.nameservers.size(), 1, 10, 11);
}

DnsSession::~DnsSession() = default;

uint16_t DnsSession::NextQueryId() const {
  return static_cast<uint16_t>(rand_callback_.Run());
}

void DnsSession::InvalidateWeakPtrsForTesting() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace net
