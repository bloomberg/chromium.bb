// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_BORINGSSL_TRUST_TOKEN_REDEMPTION_CRYPTOGRAPHER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_BORINGSSL_TRUST_TOKEN_REDEMPTION_CRYPTOGRAPHER_H_

#include "services/network/trust_tokens/trust_token_request_redemption_helper.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace network {

// Executes one instance of a Trust Tokens redemption operation by calling the
// appropriate BoringSSL methods.
class BoringsslTrustTokenRedemptionCryptographer
    : public TrustTokenRequestRedemptionHelper::Cryptographer {
 public:
  BoringsslTrustTokenRedemptionCryptographer();
  ~BoringsslTrustTokenRedemptionCryptographer() override;

  // TrustTokenRequestRedemptionHelper::Cryptographer implementation:
  bool Initialize(
      int issuer_configured_batch_size,
      base::StringPiece signed_redemption_record_verification_key) override;
  base::Optional<std::string> BeginRedemption(
      TrustToken token,
      base::StringPiece verification_key,
      const url::Origin& top_level_origin) override;
  base::Optional<std::string> ConfirmRedemption(
      base::StringPiece response_header) override;

 private:
  // Maintains Trust Tokens protocol state.
  bssl::UniquePtr<TRUST_TOKEN_CLIENT> ctx_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_BORINGSSL_TRUST_TOKEN_REDEMPTION_CRYPTOGRAPHER_H_
