// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_isolation_key_mojom_traits.h"

#include "base/unguessable_token.h"

namespace mojo {

bool StructTraits<network::mojom::NetworkIsolationKeyDataView,
                  net::NetworkIsolationKey>::
    Read(network::mojom::NetworkIsolationKeyDataView data,
         net::NetworkIsolationKey* out) {
  absl::optional<net::SchemefulSite> top_frame_site, frame_site;
  if (!data.ReadTopFrameSite(&top_frame_site))
    return false;
  if (!data.ReadFrameSite(&frame_site))
    return false;
  // A key is either fully empty or fully populated.
  if (top_frame_site.has_value() != frame_site.has_value())
    return false;
  absl::optional<base::UnguessableToken> nonce;
  if (!data.ReadNonce(&nonce))
    return false;
  if (!top_frame_site.has_value()) {
    // If there is a nonce, then the sites must be populated.
    if (nonce.has_value())
      return false;
    *out = net::NetworkIsolationKey();
  } else {
    *out = net::NetworkIsolationKey(std::move(top_frame_site.value()),
                                    std::move(frame_site.value()),
                                    nonce ? &nonce.value() : nullptr);
  }

  return true;
}

}  // namespace mojo
