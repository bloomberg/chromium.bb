// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/host_resolver.h"

#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/dns/host_resolver_source.h"
#include "net/log/net_log.h"
#include "services/network/host_resolver_mdns_listener.h"
#include "services/network/public/cpp/host_resolver_mojom_traits.h"
#include "services/network/resolve_host_request.h"

namespace network {
namespace {
static base::LazyInstance<HostResolver::ResolveHostCallback>::Leaky
    resolve_host_callback;
}

namespace {
base::Optional<net::HostResolver::ResolveHostParameters>
ConvertOptionalParameters(
    const mojom::ResolveHostParametersPtr& mojo_parameters) {
  if (!mojo_parameters)
    return base::nullopt;

  net::HostResolver::ResolveHostParameters parameters;
  parameters.dns_query_type = mojo_parameters->dns_query_type;
  parameters.initial_priority = mojo_parameters->initial_priority;
  parameters.source = mojo_parameters->source;
  switch (mojo_parameters->cache_usage) {
    case mojom::ResolveHostParameters::CacheUsage::ALLOWED:
      parameters.cache_usage =
          net::HostResolver::ResolveHostParameters::CacheUsage::ALLOWED;
      break;
    case mojom::ResolveHostParameters::CacheUsage::STALE_ALLOWED:
      parameters.cache_usage =
          net::HostResolver::ResolveHostParameters::CacheUsage::STALE_ALLOWED;
      break;
    case mojom::ResolveHostParameters::CacheUsage::DISALLOWED:
      parameters.cache_usage =
          net::HostResolver::ResolveHostParameters::CacheUsage::DISALLOWED;
      break;
  }
  parameters.include_canonical_name = mojo_parameters->include_canonical_name;
  parameters.loopback_only = mojo_parameters->loopback_only;
  parameters.is_speculative = mojo_parameters->is_speculative;
  parameters.secure_dns_mode_override = mojo::FromOptionalSecureDnsMode(
      mojo_parameters->secure_dns_mode_override);
  return parameters;
}
}  // namespace

HostResolver::HostResolver(
    mojo::PendingReceiver<mojom::HostResolver> resolver_receiver,
    ConnectionShutdownCallback connection_shutdown_callback,
    net::HostResolver* internal_resolver,
    net::NetLog* net_log)
    : receiver_(this),
      pending_receiver_(std::move(resolver_receiver)),
      connection_shutdown_callback_(std::move(connection_shutdown_callback)),
      internal_resolver_(internal_resolver),
      net_log_(net_log) {
  // Bind the pending receiver asynchronously to give the resolver a chance
  // to set up (some resolvers need to obtain the system config asynchronously).
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&HostResolver::AsyncSetUp, weak_factory_.GetWeakPtr()));
}

HostResolver::HostResolver(net::HostResolver* internal_resolver,
                           net::NetLog* net_log)
    : receiver_(this),
      internal_resolver_(internal_resolver),
      net_log_(net_log) {}

HostResolver::~HostResolver() {
  receiver_.reset();
}

void HostResolver::ResolveHost(
    const net::HostPortPair& host,
    const net::NetworkIsolationKey& network_isolation_key,
    mojom::ResolveHostParametersPtr optional_parameters,
    mojo::PendingRemote<mojom::ResolveHostClient> response_client) {
#if !BUILDFLAG(ENABLE_MDNS)
  // TODO(crbug.com/821021): Handle without crashing if we create restricted
  // HostResolvers for passing to untrusted processes.
  DCHECK(!optional_parameters ||
         optional_parameters->source != net::HostResolverSource::MULTICAST_DNS);
#endif  // !BUILDFLAG(ENABLE_MDNS)

  if (resolve_host_callback.Get())
    resolve_host_callback.Get().Run(host.host());

  auto request = std::make_unique<ResolveHostRequest>(
      internal_resolver_, host, network_isolation_key,
      ConvertOptionalParameters(optional_parameters), net_log_);

  mojo::PendingReceiver<mojom::ResolveHostHandle> control_handle_receiver;
  if (optional_parameters)
    control_handle_receiver = std::move(optional_parameters->control_handle);

  int rv = request->Start(
      std::move(control_handle_receiver), std::move(response_client),
      base::BindOnce(&HostResolver::OnResolveHostComplete,
                     base::Unretained(this), request.get()));
  if (rv != net::ERR_IO_PENDING)
    return;

  // Store the request with the resolver so it can be cancelled on resolver
  // shutdown.
  bool insertion_result = requests_.emplace(std::move(request)).second;
  DCHECK(insertion_result);
}

void HostResolver::MdnsListen(
    const net::HostPortPair& host,
    net::DnsQueryType query_type,
    mojo::PendingRemote<mojom::MdnsListenClient> response_client,
    MdnsListenCallback callback) {
#if !BUILDFLAG(ENABLE_MDNS)
  NOTREACHED();
#endif  // !BUILDFLAG(ENABLE_MDNS)

  auto listener = std::make_unique<HostResolverMdnsListener>(internal_resolver_,
                                                             host, query_type);
  int rv =
      listener->Start(std::move(response_client),
                      base::BindOnce(&HostResolver::OnMdnsListenerCancelled,
                                     base::Unretained(this), listener.get()));
  if (rv == net::OK) {
    bool insertion_result = listeners_.emplace(std::move(listener)).second;
    DCHECK(insertion_result);
  }

  std::move(callback).Run(rv);
}

size_t HostResolver::GetNumOutstandingRequestsForTesting() const {
  return requests_.size();
}

void HostResolver::SetResolveHostCallbackForTesting(
    ResolveHostCallback callback) {
  resolve_host_callback.Get() = std::move(callback);
}

void HostResolver::AsyncSetUp() {
  receiver_.Bind(std::move(pending_receiver_));
  receiver_.set_disconnect_handler(
      base::BindOnce(&HostResolver::OnConnectionError, base::Unretained(this)));
}

void HostResolver::OnResolveHostComplete(ResolveHostRequest* request,
                                         int error) {
  DCHECK_NE(net::ERR_IO_PENDING, error);

  auto found_request = requests_.find(request);
  DCHECK(found_request != requests_.end());
  requests_.erase(found_request);
}

void HostResolver::OnMdnsListenerCancelled(HostResolverMdnsListener* listener) {
  auto found_listener = listeners_.find(listener);
  DCHECK(found_listener != listeners_.end());
  listeners_.erase(found_listener);
}

void HostResolver::OnConnectionError() {
  DCHECK(connection_shutdown_callback_);

  requests_.clear();

  // Invoke last as callback may delete |this|.
  std::move(connection_shutdown_callback_).Run(this);
}

}  // namespace network
