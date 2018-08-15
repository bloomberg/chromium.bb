// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/host_resolver.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log.h"
#include "services/network/resolve_host_request.h"

namespace network {

HostResolver::HostResolver(
    mojom::HostResolverRequest resolver_request,
    ConnectionShutdownCallback connection_shutdown_callback,
    net::HostResolver* internal_resolver,
    net::NetLog* net_log)
    : binding_(this, std::move(resolver_request)),
      connection_shutdown_callback_(std::move(connection_shutdown_callback)),
      internal_resolver_(internal_resolver),
      net_log_(net_log) {
  binding_.set_connection_error_handler(
      base::BindOnce(&HostResolver::OnConnectionError, base::Unretained(this)));
}

HostResolver::HostResolver(net::HostResolver* internal_resolver,
                           net::NetLog* net_log)
    : binding_(this),
      internal_resolver_(internal_resolver),
      net_log_(net_log) {}

HostResolver::~HostResolver() {
  if (binding_)
    binding_.Close();
}

void HostResolver::ResolveHost(const net::HostPortPair& host,
                               mojom::ResolveHostHandleRequest control_handle,
                               mojom::ResolveHostClientPtr response_client) {
  auto request =
      std::make_unique<ResolveHostRequest>(internal_resolver_, host, net_log_);

  int rv =
      request->Start(std::move(control_handle), std::move(response_client),
                     base::BindOnce(&HostResolver::OnResolveHostComplete,
                                    base::Unretained(this), request.get()));
  if (rv != net::ERR_IO_PENDING)
    return;

  // Store the request with the resolver so it can be cancelled on resolver
  // shutdown.
  bool insertion_result = requests_.emplace(std::move(request)).second;
  DCHECK(insertion_result);
}

size_t HostResolver::GetNumOutstandingRequestsForTesting() const {
  return requests_.size();
}

void HostResolver::OnResolveHostComplete(ResolveHostRequest* request,
                                         int error) {
  DCHECK_NE(net::ERR_IO_PENDING, error);

  auto found_request = requests_.find(request);
  DCHECK(found_request != requests_.end());
  requests_.erase(found_request);
}

void HostResolver::OnConnectionError() {
  DCHECK(connection_shutdown_callback_);

  requests_.clear();

  // Invoke last as callback may delete |this|.
  std::move(connection_shutdown_callback_).Run(this);
}

}  // namespace network
