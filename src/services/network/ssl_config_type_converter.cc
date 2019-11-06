// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ssl_config_type_converter.h"

namespace mojo {

int MojoSSLVersionToNetSSLVersion(network::mojom::SSLVersion mojo_version) {
  switch (mojo_version) {
    case network::mojom::SSLVersion::kTLS1:
      return net::SSL_PROTOCOL_VERSION_TLS1;
    case network::mojom::SSLVersion::kTLS11:
      return net::SSL_PROTOCOL_VERSION_TLS1_1;
    case network::mojom::SSLVersion::kTLS12:
      return net::SSL_PROTOCOL_VERSION_TLS1_2;
    case network::mojom::SSLVersion::kTLS13:
      return net::SSL_PROTOCOL_VERSION_TLS1_3;
  }
  NOTREACHED();
  return net::SSL_PROTOCOL_VERSION_TLS1_3;
}

net::SSLConfig
TypeConverter<net::SSLConfig, network::mojom::SSLConfigPtr>::Convert(
    const network::mojom::SSLConfigPtr& mojo_config) {
  DCHECK(mojo_config);

  net::SSLConfig net_config;

  net_config.version_min =
      MojoSSLVersionToNetSSLVersion(mojo_config->version_min);
  net_config.version_max =
      MojoSSLVersionToNetSSLVersion(mojo_config->version_max);
  DCHECK_LE(net_config.version_min, net_config.version_max);

  for (uint16_t cipher_suite : mojo_config->disabled_cipher_suites) {
    net_config.disabled_cipher_suites.push_back(cipher_suite);
  }
  return net_config;
}

}  // namespace mojo
