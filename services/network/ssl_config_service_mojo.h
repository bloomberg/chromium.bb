// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SSL_CONFIG_SERVICE_MOJO_H_
#define SERVICES_NETWORK_SSL_CONFIG_SERVICE_MOJO_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "services/network/public/mojom/ssl_config.mojom.h"

namespace network {

// An SSLConfigClient that serves as a net::SSLConfigService, listening to
// SSLConfig changes on a Mojo pipe, and providing access to the updated config.
class COMPONENT_EXPORT(NETWORK_SERVICE) SSLConfigServiceMojo
    : public mojom::SSLConfigClient,
      public net::SSLConfigService {
 public:
  // If |ssl_config_client_request| is not provided, just sticks with the
  // initial configuration.
  SSLConfigServiceMojo(mojom::SSLConfigPtr initial_config,
                       mojom::SSLConfigClientRequest ssl_config_client_request);
  ~SSLConfigServiceMojo() override;

  // mojom::SSLConfigClient implementation:
  void OnSSLConfigUpdated(const mojom::SSLConfigPtr ssl_config) override;

  // net::SSLConfigService implementation:
  void GetSSLConfig(net::SSLConfig* ssl_config) override;
  bool CanShareConnectionWithClientCerts(
      const std::string& hostname) const override;

 private:
  mojo::Binding<mojom::SSLConfigClient> binding_;

  net::SSLConfig ssl_config_;

  // The list of domains and subdomains from enterprise policy where connection
  // coalescing is allowed when client certs are in use if the hosts being
  // coalesced match this list.
  std::vector<std::string> client_cert_pooling_policy_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServiceMojo);
};

}  // namespace network

#endif  // SERVICES_NETWORK_SSL_CONFIG_SERVICE_MOJO_H_
