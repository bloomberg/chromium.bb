// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PROXY_CONFIGURATOR_H_
#define CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PROXY_CONFIGURATOR_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/proxy_server.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

// Configures the use of the IP-masking CONNECT tunnel proxy for Isolated
// Prerenders, and observers their connection failures.
class PrefetchProxyProxyConfigurator
    : public network::mojom::CustomProxyConnectionObserver {
 public:
  PrefetchProxyProxyConfigurator();
  ~PrefetchProxyProxyConfigurator() override;

  // Adds a config client that can be used to update Data Reduction Proxy
  // settings.
  void AddCustomProxyConfigClient(
      mojo::Remote<network::mojom::CustomProxyConfigClient> config_client,
      base::OnceCallback<void()> callback);

  // Updates the custom proxy config to all clients.
  void UpdateCustomProxyConfig(
      base::OnceCallback<void()> callback);

  // Creates a config that can be sent to the NetworkContext.
  network::mojom::CustomProxyConfigPtr CreateCustomProxyConfig() const;

  // Passes a new pending remote for the custom proxy connection observer
  // implemented by this class.
  mojo::PendingRemote<network::mojom::CustomProxyConnectionObserver>
  NewProxyConnectionObserverRemote();

  // Returns true when the prefetch proxy has not recently had any connection
  // errors.
  bool IsPrefetchProxyAvailable() const;

  void SetClockForTesting(const base::Clock* clock);

  // mojom::CustomProxyConnectionObserver:
  void OnFallback(const net::ProxyServer& bad_proxy, int net_error) override;
  void OnTunnelHeadersReceived(
      const net::ProxyServer& proxy_server,
      const scoped_refptr<net::HttpResponseHeaders>& response_headers) override;

 private:
  // Called when an error is detected by the CustomProxyConnectionObserver
  // implementation so that we can throttle requests to the proxy.
  void OnTunnelProxyConnectionError(
      absl::optional<base::TimeDelta> retry_after);

  // The headers used to setup the connect tunnel.
  net::HttpRequestHeaders connect_tunnel_headers_;

  // The proxy server used for prefetch requests.
  const net::ProxyServer prefetch_proxy_server_;

  // The time clock used to calculate |prefetch_proxy_not_available_until_|.
  raw_ptr<const base::Clock> clock_;

  // If set, the prefetch proxy should not be used until this time.
  absl::optional<base::Time> prefetch_proxy_not_available_until_;

  // The set of clients that will get updates about changes to the proxy config.
  mojo::RemoteSet<network::mojom::CustomProxyConfigClient>
      proxy_config_clients_;

  // The set of receivers for tunnel connection observers.
  mojo::ReceiverSet<network::mojom::CustomProxyConnectionObserver>
      observer_receivers_;

  SEQUENCE_CHECKER(sequence_checker_);

  PrefetchProxyProxyConfigurator(const PrefetchProxyProxyConfigurator&) =
      delete;
  PrefetchProxyProxyConfigurator& operator=(
      const PrefetchProxyProxyConfigurator&) = delete;
};

#endif  // CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_PROXY_PROXY_CONFIGURATOR_H_
