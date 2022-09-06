// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CONTENT_BROWSER_RENDERER_HOST_PRIVATE_NETWORK_ACCESS_UTIL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PRIVATE_NETWORK_ACCESS_UTIL_H_

#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace content {

class ContentBrowserClient;
struct PolicyContainerPolicies;

// Returns the policy to use for private network requests fetched by a client
// with the given context properties.
//
// `ip_address_space` identifies the IP address space of the request client.
// `is_web_secure_context` specifies whether the request client is a secure
// context or not.
network::mojom::PrivateNetworkRequestPolicy DerivePrivateNetworkRequestPolicy(
    network::mojom::IPAddressSpace ip_address_space,
    bool is_web_secure_context);

// Convenience overload to directly compute this from the client's `policies`.
network::mojom::PrivateNetworkRequestPolicy DerivePrivateNetworkRequestPolicy(
    const PolicyContainerPolicies& policies);

// Determines the IP address space that should be associated to execution
// contexts instantiated from a resource loaded from this `url` and the given
// response.
//
// This differs from `network::CalculateClientAddressSpace()` in that it takes
// into account special schemes that are only known to the embedder, for which
// the embedder decides the IP address space.
//
// `url` the response URL.
// `response_head` identifies the response headers received which may be
// nullptr. `client` exposes the embedder API which may be nullptr.
network::mojom::IPAddressSpace CalculateIPAddressSpace(
    const GURL& url,
    network::mojom::URLResponseHead* response_head,
    ContentBrowserClient* client);
}  // namespace content

#endif  //  CONTENT_BROWSER_RENDERER_HOST_PRIVATE_NETWORK_ACCESS_UTIL_H_
