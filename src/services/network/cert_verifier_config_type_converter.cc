// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cert_verifier_config_type_converter.h"

namespace mojo {

net::CertVerifier::Config
TypeConverter<net::CertVerifier::Config, network::mojom::SSLConfigPtr>::Convert(
    const network::mojom::SSLConfigPtr& mojo_config) {
  DCHECK(mojo_config);

  net::CertVerifier::Config net_config;
  net_config.enable_rev_checking = mojo_config->rev_checking_enabled;
  net_config.require_rev_checking_local_anchors =
      mojo_config->rev_checking_required_local_anchors;
  net_config.enable_sha1_local_anchors =
      mojo_config->sha1_local_anchors_enabled;
  net_config.disable_symantec_enforcement =
      mojo_config->symantec_enforcement_disabled;

  return net_config;
}

}  // namespace mojo
