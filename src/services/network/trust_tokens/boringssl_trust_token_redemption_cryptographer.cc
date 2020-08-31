// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/boringssl_trust_token_redemption_cryptographer.h"

#include "base/base64.h"
#include "base/callback_helpers.h"
#include "net/http/structured_headers.h"
#include "services/network/trust_tokens/scoped_boringssl_bytes.h"
#include "services/network/trust_tokens/signed_redemption_record_serialization.h"
#include "services/network/trust_tokens/trust_token_client_data_canonicalization.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/trust_token.h"

namespace network {

BoringsslTrustTokenRedemptionCryptographer::
    BoringsslTrustTokenRedemptionCryptographer() = default;

BoringsslTrustTokenRedemptionCryptographer::
    ~BoringsslTrustTokenRedemptionCryptographer() = default;

bool BoringsslTrustTokenRedemptionCryptographer::Initialize(
    int issuer_configured_batch_size,
    base::StringPiece signed_redemption_record_verification_key) {
  if (!base::IsValueInRangeForNumericType<size_t>(issuer_configured_batch_size))
    return false;

  ctx_ = bssl::UniquePtr<TRUST_TOKEN_CLIENT>(TRUST_TOKEN_CLIENT_new(
      TRUST_TOKEN_experiment_v1(),
      static_cast<size_t>(issuer_configured_batch_size)));
  if (!ctx_)
    return false;

  bssl::UniquePtr<EVP_PKEY> srr_verification_pkey(EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519,
      /*unused=*/nullptr,  // Yes, this parameter is called "unused".
      base::as_bytes(base::make_span(signed_redemption_record_verification_key))
          .data(),
      signed_redemption_record_verification_key.size()));
  if (!srr_verification_pkey)
    return false;

  if (!TRUST_TOKEN_CLIENT_set_srr_key(ctx_.get(),
                                      srr_verification_pkey.get())) {
    return false;
  }

  return true;
}

base::Optional<std::string>
BoringsslTrustTokenRedemptionCryptographer::BeginRedemption(
    TrustToken token,
    base::StringPiece verification_key_to_bind,
    const url::Origin& top_level_origin) {
  if (!ctx_)
    return base::nullopt;

  // It's unclear if BoringSSL expects the exact same value in the client data
  // and as the |time| argument to TRUST_TOKEN_CLIENT_begin_redemption; play
  // it safe by using a single timestamp.
  base::Time redemption_timestamp = base::Time::Now();

  base::Optional<std::vector<uint8_t>> maybe_client_data =
      CanonicalizeTrustTokenClientDataForRedemption(
          redemption_timestamp, top_level_origin, verification_key_to_bind);
  if (!maybe_client_data)
    return base::nullopt;

  ScopedBoringsslBytes raw_redemption_request;

  bssl::UniquePtr<TRUST_TOKEN> boringssl_token(
      TRUST_TOKEN_new(base::as_bytes(base::make_span(token.body())).data(),
                      token.body().size()));
  if (!boringssl_token)
    return base::nullopt;

  if (!TRUST_TOKEN_CLIENT_begin_redemption(
          ctx_.get(), raw_redemption_request.mutable_ptr(),
          raw_redemption_request.mutable_len(), boringssl_token.get(),
          maybe_client_data->data(), maybe_client_data->size(),
          (redemption_timestamp - base::Time::UnixEpoch()).InSeconds())) {
    return base::nullopt;
  }

  return base::Base64Encode(raw_redemption_request.as_span());
}

base::Optional<std::string>
BoringsslTrustTokenRedemptionCryptographer::ConfirmRedemption(
    base::StringPiece response_header) {
  if (!ctx_)
    return base::nullopt;

  std::string decoded_response;
  if (!base::Base64Decode(response_header, &decoded_response))
    return base::nullopt;

  // |srr_body| and |srr_signature| together provide the information in the
  // signed redemption record (see ConstructSignedRedemptionRecord's function
  // comment for more information).
  ScopedBoringsslBytes srr_body;
  ScopedBoringsslBytes srr_signature;
  if (!TRUST_TOKEN_CLIENT_finish_redemption(
          ctx_.get(), srr_body.mutable_ptr(), srr_body.mutable_len(),
          srr_signature.mutable_ptr(), srr_signature.mutable_len(),
          base::as_bytes(base::make_span(decoded_response)).data(),
          decoded_response.size())) {
    return base::nullopt;
  }

  return ConstructSignedRedemptionRecord(srr_body.as_span(),
                                         srr_signature.as_span());
}

}  // namespace network
