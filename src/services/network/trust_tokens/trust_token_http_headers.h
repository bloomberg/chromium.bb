// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_HTTP_HEADERS_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_HTTP_HEADERS_H_

#include <vector>

#include "base/strings/string_piece_forward.h"

namespace network {

// These are the HTTP headers defined in the Trust Tokens draft explainer:
// https://github.com/WICG/trust-token-api
//
// NOTE: If you add more request headers, please make sure to update the
// definition of |TrustTokensRequestHeaders|.

// As a request header: during issuance, sends a collection of unsigned, blinded
// tokens; during redemption, sends a single signed, unblinded token
// along with associated redemption metadata.
// As a response header: during issuance, provides a collection of signed,
// blinded tokens; during redemption, includes a just-created Signed Redemption
// Record.
constexpr char kTrustTokensSecTrustTokenHeader[] = "Sec-Trust-Token";

// As a request header, provides a timestamp associated with a
// particular Trust Tokens signature-bearing request.
constexpr char kTrustTokensRequestHeaderSecTime[] = "Sec-Time";

// As a request header, provides a signature over the canonical record
// associated with a given request (containing the request's URL; optionally, a
// collection of headers; and, optionally, the request's body).
constexpr char kTrustTokensRequestHeaderSecSignature[] = "Sec-Signature";

// As a request header, provides a Signed Redemption Record obtained from a
// prior issuance-and-redemption flow.
constexpr char kTrustTokensRequestHeaderSecSignedRedemptionRecord[] =
    "Sec-Signed-Redemption-Record";

// As a request header during the request signing operation, provides the list
// of headers included in the signing data's canonical request data. An absent
// header denotes an empty list.
constexpr char kTrustTokensRequestHeaderSignedHeaders[] = "Signed-Headers";

// Returns a view of all of the Trust Tokens-internal request headers.
// This vector contains all of the headers that clients must not provide on
// requests bearing Trust Tokens operations, because they are added internally
// by Trust Tokens logic.
//
// In particular, this does *not* contain Signed-Headers because this header's
// value is provided by the Trust Token API's client.
const std::vector<base::StringPiece>& TrustTokensRequestHeaders();

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TRUST_TOKEN_HTTP_HEADERS_H_
