// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_HOST_RESOLVER_H_
#define SERVICES_NETWORK_HOST_RESOLVER_H_

#include <memory>
#include <set>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace net {
class HostResolver;
class HostPortPair;
class NetLog;
}  // namespace net

namespace network {
class ResolveHostRequest;

class COMPONENT_EXPORT(NETWORK_SERVICE) HostResolver
    : public mojom::HostResolver {
 public:
  using ConnectionShutdownCallback = base::OnceCallback<void(HostResolver*)>;

  // Constructs and binds to the given mojom::HostResolver pipe. On pipe close,
  // cancels all outstanding requests (whether made through the pipe or by
  // directly calling ResolveHost()) with ERR_FAILED. Also on pipe close, calls
  // |connection_shutdown_callback| and passes |this| to notify that the
  // resolver has cancelled all requests and may be cleaned up.
  HostResolver(mojom::HostResolverRequest resolver_request,
               ConnectionShutdownCallback connection_shutdown_callback,
               net::HostResolver* internal_resolver,
               net::NetLog* net_log);
  // Constructor for when the resolver will not be bound to a
  // mojom::HostResolver pipe, eg because it is handling ResolveHost requests
  // made directly on NetworkContext.
  HostResolver(net::HostResolver* internal_resolver, net::NetLog* net_log);
  ~HostResolver() override;

  void ResolveHost(const net::HostPortPair& host,
                   mojom::ResolveHostHandleRequest control_handle,
                   mojom::ResolveHostClientPtr response_client) override;

  size_t GetNumOutstandingRequestsForTesting() const;

 private:
  void OnResolveHostComplete(ResolveHostRequest* request, int error);
  void OnConnectionError();

  mojo::Binding<mojom::HostResolver> binding_;
  ConnectionShutdownCallback connection_shutdown_callback_;
  std::set<std::unique_ptr<ResolveHostRequest>, base::UniquePtrComparator>
      requests_;

  net::HostResolver* const internal_resolver_;
  net::NetLog* const net_log_;

  DISALLOW_COPY_AND_ASSIGN(HostResolver);
};

}  // namespace network

#endif  // SERVICES_NETWORK_HOST_RESOLVER_H_
