// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "base/time/time_to_iso8601.h"
#include "services/network/trust_tokens/boringssl_trust_token_issuance_cryptographer.h"
#include "services/network/trust_tokens/boringssl_trust_token_redemption_cryptographer.h"
#include "services/network/trust_tokens/scoped_boringssl_bytes.h"
#include "services/network/trust_tokens/test/signed_request_verification_util.h"
#include "services/network/trust_tokens/trust_token_client_data_canonicalization.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/trust_token.h"
#include "url/gurl.h"
#include "url/origin.h"

// These integration tests confirm that BoringsslTrustToken{Issuance,
// Redemption}Cryptographer are capable of completing an end-to-end
// issuance-and-redemption flow against the server-side BoringSSL issuer logic.

namespace network {

namespace {

struct TokenKeyPair {
  // Token signing and verification keys, along with an associated ID that we
  // can pass to tell BoringSSL which key to use for issuance.
  std::vector<uint8_t> signing;
  std::vector<uint8_t> verification;
  uint32_t key_id;
};
struct ProtocolKeys {
  std::vector<TokenKeyPair> token_keys;

  // Signed redemption record (SRR) signing and verification keys:
  std::vector<uint8_t> srr_signing;
  std::vector<uint8_t> srr_verification;
};

// Choose this number to be > 1 but fairly small: setting it to 10
// led to the test running for 2.5 sec on a debug build.
constexpr size_t kNumTokensToRequest = 3;

// Generates a token signing and verification key pair associated with the given
// key ID.
TokenKeyPair GenerateTokenKeys(uint32_t key_id) {
  TokenKeyPair keys;
  keys.key_id = key_id;

  keys.signing.resize(TRUST_TOKEN_MAX_PRIVATE_KEY_SIZE);
  keys.verification.resize(TRUST_TOKEN_MAX_PUBLIC_KEY_SIZE);
  size_t signing_key_len, verification_key_len;
  CHECK(TRUST_TOKEN_generate_key(
      TRUST_TOKEN_experiment_v1(), keys.signing.data(), &signing_key_len,
      keys.signing.size(), keys.verification.data(), &verification_key_len,
      keys.verification.size(), key_id));
  keys.signing.resize(signing_key_len);
  keys.verification.resize(verification_key_len);

  return keys;
}

// Generates a set of issuance keys associated with |key_id| and a set of signed
// redemption record keys.
ProtocolKeys GenerateProtocolKeys(size_t num_keys) {
  ProtocolKeys keys;
  for (size_t i = 0; i < num_keys; ++i) {
    // Use key IDs 0, 3, 6, ... to ensure that no logic relies on key IDs
    // being consecutive.
    keys.token_keys.push_back(GenerateTokenKeys(/*key_id=*/3 * i));
  }

  keys.srr_signing.resize(ED25519_PRIVATE_KEY_LEN);
  keys.srr_verification.resize(ED25519_PUBLIC_KEY_LEN);
  // (ED25519_keypair can't fail).
  ED25519_keypair(keys.srr_verification.data(), keys.srr_signing.data());

  return keys;
}

base::StringPiece as_string(base::span<const uint8_t> bytes) {
  return base::StringPiece(reinterpret_cast<const char*>(bytes.data()),
                           bytes.size());
}

base::StringPiece as_string(const std::vector<uint8_t>& bytes) {
  return as_string(base::make_span(bytes));
}

// If RequestManyTokensAndRetainOneArbitrarily doesn't throw a fatal failure,
// then issuance succeeded and |out_token|'s now contains a token
// suitable for performing a redemption (with the same issuer keys).
//
// The issuer-side logic will use |key_with_which_to_issue| to issue the token;
// this must be a reference to a member of |keys.token_keys|.
//
// Note: This method returns void so that it's possible to ASSERT inside it.
void RequestManyTokensAndRetainOneArbitrarily(
    const ProtocolKeys& keys,
    const TokenKeyPair& key_with_which_to_issue,
    TRUST_TOKEN_ISSUER* issuer_ctx,
    TrustToken* out_token) {
  BoringsslTrustTokenIssuanceCryptographer issuance_cryptographer;
  ASSERT_TRUE(issuance_cryptographer.Initialize(kNumTokensToRequest));

  for (const TokenKeyPair& token_keys : keys.token_keys) {
    ASSERT_TRUE(issuance_cryptographer.AddKey(std::string(
        reinterpret_cast<const char*>(token_keys.verification.data()),
        token_keys.verification.size())));
  }

  base::Optional<std::string> maybe_base64_encoded_issuance_request =
      issuance_cryptographer.BeginIssuance(kNumTokensToRequest);
  ASSERT_TRUE(maybe_base64_encoded_issuance_request);

  std::string raw_issuance_request;
  ASSERT_TRUE(base::Base64Decode(*maybe_base64_encoded_issuance_request,
                                 &raw_issuance_request));

  constexpr uint8_t kPrivateMetadata = 0;
  ScopedBoringsslBytes raw_issuance_response;
  size_t num_tokens_issued;
  ASSERT_TRUE(TRUST_TOKEN_ISSUER_issue(
      issuer_ctx, raw_issuance_response.mutable_ptr(),
      raw_issuance_response.mutable_len(), &num_tokens_issued,
      base::as_bytes(base::make_span(raw_issuance_request)).data(),
      raw_issuance_request.size(), key_with_which_to_issue.key_id,
      kPrivateMetadata, kNumTokensToRequest));

  std::unique_ptr<
      TrustTokenRequestIssuanceHelper::Cryptographer::UnblindedTokens>
      obtained_tokens = issuance_cryptographer.ConfirmIssuance(
          base::Base64Encode(raw_issuance_response.as_span()));
  ASSERT_TRUE(obtained_tokens);
  ASSERT_EQ(obtained_tokens->tokens.size(), kNumTokensToRequest);
  ASSERT_EQ(obtained_tokens->body_of_verifying_key,
            as_string(key_with_which_to_issue.verification));

  out_token->set_body(obtained_tokens->tokens.front());
  out_token->set_signing_key(obtained_tokens->body_of_verifying_key);
}

// Uses a RedemptionCryptographer to construct a redemption request wrapping
// |token_to_redeem|; verifies that the server-side BoringSSL redemption code
// accepts the redemption request, and that the RedemptionCryptographer
// correctly handles the corresponding redemption response.
void RedeemSingleToken(const ProtocolKeys& keys,
                       TRUST_TOKEN_ISSUER* issuer_ctx,
                       const TrustToken& token_to_redeem,
                       std::string* signed_redemption_record_out) {
  BoringsslTrustTokenRedemptionCryptographer redemption_cryptographer;

  const url::Origin kRedeemingOrigin =
      url::Origin::Create(GURL("https://topframe.example"));

  ASSERT_TRUE(redemption_cryptographer.Initialize(
      kNumTokensToRequest, as_string(keys.srr_verification)));

  base::Optional<std::string> maybe_base64_encoded_redemption_request =
      redemption_cryptographer.BeginRedemption(
          token_to_redeem,
          "client-generated public key bound to the redemption",
          kRedeemingOrigin);

  ASSERT_TRUE(maybe_base64_encoded_redemption_request);

  std::string raw_redemption_request;
  ASSERT_TRUE(base::Base64Decode(*maybe_base64_encoded_redemption_request,
                                 &raw_redemption_request));

  // OPENSSL_EXPORT int TRUST_TOKEN_ISSUER_redeem(
  //  const TRUST_TOKEN_ISSUER *ctx, uint8_t **out, size_t *out_len,
  //  TRUST_TOKEN **out_token, uint8_t **out_client_data,
  //  size_t *out_client_data_len, uint64_t *out_redemption_time,
  //  const uint8_t *request, size_t request_len, uint64_t lifetime);
  constexpr base::TimeDelta kSrrLifetime = base::TimeDelta::FromSeconds(100);
  ScopedBoringsslBytes raw_redemption_response;
  TRUST_TOKEN* redeemed_token;
  ScopedBoringsslBytes redeemed_client_data;
  uint64_t received_redemption_timestamp;
  ASSERT_TRUE(TRUST_TOKEN_ISSUER_redeem(
      issuer_ctx, raw_redemption_response.mutable_ptr(),
      raw_redemption_response.mutable_len(), &redeemed_token,
      redeemed_client_data.mutable_ptr(), redeemed_client_data.mutable_len(),
      &received_redemption_timestamp,
      base::as_bytes(base::make_span(raw_redemption_request)).data(),
      raw_redemption_request.size(), kSrrLifetime.InSeconds()));
  // Put the issuer-receied token in a smart pointer so it will get deleted on
  // leaving scope.
  bssl::UniquePtr<TRUST_TOKEN> redeemed_token_scoper(redeemed_token);

  // Note: It might seem reasonable to compare |redeemed_token| with
  // |token_to_redeem|, but the TRUST_TOKEN type represents different things in
  // client and server settings, so these structs don't necessarily have equal
  // contents.
  EXPECT_EQ(base::checked_cast<int64_t>(received_redemption_timestamp),
            (base::Time::Now() - base::Time::UnixEpoch()).InSeconds());
  auto expected_client_data = *CanonicalizeTrustTokenClientDataForRedemption(
      base::Time::Now(), kRedeemingOrigin,
      "client-generated public key bound to the redemption");
  EXPECT_TRUE(std::equal(redeemed_client_data.as_span().begin(),
                         redeemed_client_data.as_span().end(),
                         expected_client_data.begin(),
                         expected_client_data.end()));

  std::string base64_encoded_redemption_response =
      base::Base64Encode(raw_redemption_response.as_span());
  base::Optional<std::string> maybe_signed_redemption_record =
      redemption_cryptographer.ConfirmRedemption(
          base64_encoded_redemption_response);

  ASSERT_TRUE(maybe_signed_redemption_record);
  signed_redemption_record_out->swap(*maybe_signed_redemption_record);
}

}  // namespace

TEST(TrustTokenCryptographersTest, IssuanceAndRedemption) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  ProtocolKeys keys = GenerateProtocolKeys(/*num_keys=*/1);

  // Initialization: provide the issuer context the token-signing and
  // SRR-signing keys.
  bssl::UniquePtr<TRUST_TOKEN_ISSUER> issuer_ctx(TRUST_TOKEN_ISSUER_new(
      TRUST_TOKEN_experiment_v1(), /*max_batchsize=*/kNumTokensToRequest));
  ASSERT_TRUE(issuer_ctx);
  for (const TokenKeyPair& token_key_pair : keys.token_keys) {
    ASSERT_TRUE(TRUST_TOKEN_ISSUER_add_key(issuer_ctx.get(),
                                           token_key_pair.signing.data(),
                                           token_key_pair.signing.size()));
  }
  // Copying the comment from evp.h:
  // The [Ed25519] RFC 8032 private key format is the 32-byte prefix of
  // |ED25519_sign|'s 64-byte private key.
  bssl::UniquePtr<EVP_PKEY> issuer_srr_key(EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, /*unused=*/nullptr, keys.srr_signing.data(),
      /*len=*/32));
  ASSERT_TRUE(issuer_srr_key);
  ASSERT_TRUE(
      TRUST_TOKEN_ISSUER_set_srr_key(issuer_ctx.get(), issuer_srr_key.get()));

  // Execute an issuance and a redemption; they should succeed.
  TrustToken token;
  ASSERT_NO_FATAL_FAILURE(RequestManyTokensAndRetainOneArbitrarily(
      keys, keys.token_keys[0], issuer_ctx.get(), &token));

  std::string signed_redemption_record;
  ASSERT_NO_FATAL_FAILURE(RedeemSingleToken(keys, issuer_ctx.get(), token,
                                            &signed_redemption_record));

  ASSERT_EQ(test::VerifyTrustTokenSignedRedemptionRecord(
                signed_redemption_record, as_string(keys.srr_verification)),
            test::SrrVerificationStatus::kSuccess);
}

TEST(TrustTokenCryptographersTest, IssuanceAndRedemptionWithMultipleKeys) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  ProtocolKeys keys = GenerateProtocolKeys(/*num_keys=*/3);

  // Initialization: provide the issuer context the token-signing and
  // SRR-signing keys.
  bssl::UniquePtr<TRUST_TOKEN_ISSUER> issuer_ctx(TRUST_TOKEN_ISSUER_new(
      TRUST_TOKEN_experiment_v1(), /*max_batchsize=*/kNumTokensToRequest));
  ASSERT_TRUE(issuer_ctx);
  for (const TokenKeyPair& token_key_pair : keys.token_keys) {
    ASSERT_TRUE(TRUST_TOKEN_ISSUER_add_key(issuer_ctx.get(),
                                           token_key_pair.signing.data(),
                                           token_key_pair.signing.size()));
  }
  // Copying the comment from evp.h:
  // The [Ed25519] RFC 8032 private key format is the 32-byte prefix of
  // |ED25519_sign|'s 64-byte private key.
  bssl::UniquePtr<EVP_PKEY> issuer_srr_key(EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, /*unused=*/nullptr, keys.srr_signing.data(),
      /*len=*/32));
  ASSERT_TRUE(issuer_srr_key);
  ASSERT_TRUE(
      TRUST_TOKEN_ISSUER_set_srr_key(issuer_ctx.get(), issuer_srr_key.get()));

  // Issuance should succeed when the issuer uses the first issuance key
  // generated, and when the issuer uses the second issuance key generated.
  TrustToken token;
  ASSERT_NO_FATAL_FAILURE(RequestManyTokensAndRetainOneArbitrarily(
      keys, keys.token_keys[0], issuer_ctx.get(), &token));
  TrustToken another_token;
  ASSERT_NO_FATAL_FAILURE(RequestManyTokensAndRetainOneArbitrarily(
      keys, keys.token_keys[1], issuer_ctx.get(), &another_token));

  // In both cases, redeeming a token from the issuance should succeed and yield
  // a well-formed signed redemption record.
  std::string signed_redemption_record;
  ASSERT_NO_FATAL_FAILURE(RedeemSingleToken(keys, issuer_ctx.get(), token,
                                            &signed_redemption_record));

  ASSERT_EQ(test::VerifyTrustTokenSignedRedemptionRecord(
                signed_redemption_record, as_string(keys.srr_verification)),
            test::SrrVerificationStatus::kSuccess);

  ASSERT_NO_FATAL_FAILURE(RedeemSingleToken(
      keys, issuer_ctx.get(), another_token, &signed_redemption_record));

  ASSERT_EQ(test::VerifyTrustTokenSignedRedemptionRecord(
                signed_redemption_record, as_string(keys.srr_verification)),
            test::SrrVerificationStatus::kSuccess);
}

}  // namespace network
