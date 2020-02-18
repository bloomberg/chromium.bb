// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_RESOLVE_HOST_REQUEST_H_
#define SERVICES_NETWORK_RESOLVE_HOST_REQUEST_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "net/dns/host_resolver.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

namespace net {
class HostPortPair;
class NetLog;
class NetworkIsolationKey;
}  // namespace net

namespace network {

// Manager of a single Mojo request to NetworkContext::ResolveHost(). Binds
// itself as the implementation of the control handle, and manages request
// lifetime and cancellation.
class ResolveHostRequest : public mojom::ResolveHostHandle {
 public:
  ResolveHostRequest(
      net::HostResolver* resolver,
      const net::HostPortPair& host,
      const net::NetworkIsolationKey& network_isolation_key,
      const base::Optional<net::HostResolver::ResolveHostParameters>&
          optional_parameters,
      net::NetLog* net_log);
  ~ResolveHostRequest() override;

  int Start(
      mojo::PendingReceiver<mojom::ResolveHostHandle> control_handle_request,
      mojo::PendingRemote<mojom::ResolveHostClient> pending_response_client,
      net::CompletionOnceCallback callback);

  // ResolveHostHandle overrides.
  void Cancel(int error) override;

 private:
  void OnComplete(int error);
  net::ResolveErrorInfo GetResolveErrorInfo() const;
  const base::Optional<net::AddressList>& GetAddressResults() const;
  void SignalNonAddressResults();

  std::unique_ptr<net::HostResolver::ResolveHostRequest> internal_request_;

  mojo::Receiver<mojom::ResolveHostHandle> control_handle_receiver_{this};
  mojo::Remote<mojom::ResolveHostClient> response_client_;
  net::CompletionOnceCallback callback_;
  bool cancelled_ = false;
  // Error info for a cancelled request.
  net::ResolveErrorInfo resolve_error_info_;

  DISALLOW_COPY_AND_ASSIGN(ResolveHostRequest);
};

}  // namespace network

#endif  // SERVICES_NETWORK_RESOLVE_HOST_REQUEST_H_
