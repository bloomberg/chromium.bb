// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_NETWORK_CONTEXT_H_
#define CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_NETWORK_CONTEXT_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/auth.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "services/network/public/mojom/auth_and_certificate_observer.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace chromeos {

class WilcoDtcSupportdNetworkContext {
 public:
  virtual ~WilcoDtcSupportdNetworkContext() = default;

  virtual network::mojom::URLLoaderFactory* GetURLLoaderFactory() = 0;
};

class WilcoDtcSupportdNetworkContextImpl
    : public WilcoDtcSupportdNetworkContext,
      public network::mojom::AuthenticationAndCertificateObserver {
 public:
  WilcoDtcSupportdNetworkContextImpl();
  ~WilcoDtcSupportdNetworkContextImpl() override;

  // WilcoDtcSupportdNetworkContext overrides:
  network::mojom::URLLoaderFactory* GetURLLoaderFactory() override;

  void FlushForTesting();

 private:
  // Ensures that Network Context created and connected to the network service.
  void EnsureNetworkContextExists();

  // Creates Network Context.
  void CreateNetworkContext();

  // network::mojom::AuthenticationAndCertificateObserver interface.
  void OnSSLCertificateError(const GURL& url,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;
  void OnCertificateRequested(
      const base::Optional<base::UnguessableToken>& window_id,
      const scoped_refptr<net::SSLCertRequestInfo>& cert_info,
      mojo::PendingRemote<network::mojom::ClientCertificateResponder>
          cert_responder) override;
  void OnAuthRequired(
      const base::Optional<base::UnguessableToken>& window_id,
      uint32_t request_id,
      const GURL& url,
      bool first_auth_attempt,
      const net::AuthChallengeInfo& auth_info,
      const scoped_refptr<net::HttpResponseHeaders>& head_headers,
      mojo::PendingRemote<network::mojom::AuthChallengeResponder>
          auth_challenge_responder) override;
  void Clone(mojo::PendingReceiver<
             network::mojom::AuthenticationAndCertificateObserver> listener)
      override;

  ProxyConfigMonitor proxy_config_monitor_;

  // NetworkContext using the network service.
  mojo::Remote<network::mojom::NetworkContext> network_context_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  mojo::ReceiverSet<network::mojom::AuthenticationAndCertificateObserver>
      cert_receivers_;

  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdNetworkContextImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_WILCO_DTC_SUPPORTD_NETWORK_CONTEXT_H_
