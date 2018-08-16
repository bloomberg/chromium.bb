// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CERT_VERIFIER_CONFIG_TYPE_CONVERTER_H_
#define SERVICES_NETWORK_CERT_VERIFIER_CONFIG_TYPE_CONVERTER_H_

#include "mojo/public/cpp/bindings/type_converter.h"
#include "net/cert/cert_verifier.h"
#include "services/network/public/mojom/ssl_config.mojom.h"

namespace mojo {

// Converts a network::mojom::SSLConfigPtr to a net::CertVerifier::Config.
// Tested in SSLConfigServiceMojo's unittests.
template <>
struct TypeConverter<net::CertVerifier::Config, network::mojom::SSLConfigPtr> {
  static net::CertVerifier::Config Convert(
      const network::mojom::SSLConfigPtr& mojo_config);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_CERT_VERIFIER_CONFIG_TYPE_CONVERTER_H_
