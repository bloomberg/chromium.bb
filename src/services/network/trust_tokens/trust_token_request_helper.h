// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_HELPER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_HELPER_H_

#include "base/callback_forward.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace net {
class URLRequest;
}  // namespace net

namespace network {

// A request helper coordinates most externally-visible, high-level Trust Token
// logic and state changes. It knows how to execute pre- and post-request logic
// for a single Trust Token operation (issuance, redemption, or signing and
// attaching cached redemption records).
class TrustTokenRequestHelper {
 public:
  TrustTokenRequestHelper() = default;
  virtual ~TrustTokenRequestHelper() = default;

  TrustTokenRequestHelper(const TrustTokenRequestHelper&) = delete;
  TrustTokenRequestHelper& operator=(const TrustTokenRequestHelper&) = delete;

  // Checks preconditions for |request| and the protocol operation
  // that this TrustTokenRequestHelper is responsible for. This completes
  // asynchronously because it might take a long time (for instance, for Trust
  // Tokens issuance and redemption, this involves executing a network request).
  virtual void Begin(
      net::URLRequest* request,
      base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) = 0;

  // Checks |response| for issuance response headers; if these are present and
  // valid, removes the headers, updates internal protocol state, and returns
  // true. Otherwise, returns false.
  virtual void Finalize(
      mojom::URLResponseHead* response,
      base::OnceCallback<void(mojom::TrustTokenOperationStatus)> done) = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_REQUEST_HELPER_H_
