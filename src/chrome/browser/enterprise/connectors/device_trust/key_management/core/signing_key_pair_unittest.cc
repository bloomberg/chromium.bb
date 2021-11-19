// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"

#include <memory>
#include <vector>

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mock_key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/scoped_key_persistence_delegate_factory.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using BPKUP = enterprise_management::BrowserPublicKeyUploadResponse;
using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
using testing::Return;

namespace enterprise_connectors {

namespace {

void ValidateSigningKey(const absl::optional<SigningKeyPair>& key_pair,
                        BPKUR::KeyTrustLevel expected_trust_level) {
  ASSERT_TRUE(key_pair);

  EXPECT_EQ(expected_trust_level, key_pair->trust_level());
  ASSERT_TRUE(key_pair->key());

  // Extract a pubkey should work.
  std::vector<uint8_t> pubkey = key_pair->key()->GetSubjectPublicKeyInfo();
  ASSERT_GT(pubkey.size(), 0u);

  // Signing should work.
  auto signed_data = key_pair->key()->SignSlowly(
      base::as_bytes(base::make_span("data to sign")));
  ASSERT_TRUE(signed_data.has_value());
  ASSERT_GT(signed_data->size(), 0u);
}

}  // namespace

// Tests that the SigningKeyPair::Create factory function returns nothing if no
// key was persisted.
TEST(SigningKeyPairTest, Create_NoKey) {
  testing::StrictMock<test::MockKeyPersistenceDelegate>
      mock_persistence_delegate;
  EXPECT_CALL(mock_persistence_delegate, LoadKeyPair())
      .WillOnce(Return(KeyPersistenceDelegate::KeyInfo(
          BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>())));
  EXPECT_FALSE(SigningKeyPair::Create(&mock_persistence_delegate));
}

// Tests that the SigningKeyPair::Create factory function returns a properly
// initialized TPM-backed SigningKeyPair if a TPM-backed key was available.
TEST(SigningKeyPairTest, Create_WithTpmKey) {
  // The mocked factory returns mock delegates setup with TPM key pairs by
  // default.
  test::ScopedKeyPersistenceDelegateFactory factory;
  auto mocked_delegate = factory.CreateMockedDelegate();

  EXPECT_CALL(*mocked_delegate, LoadKeyPair());
  EXPECT_CALL(*mocked_delegate, GetTpmBackedKeyProvider());

  absl::optional<SigningKeyPair> key_pair =
      SigningKeyPair::Create(mocked_delegate.get());

  ValidateSigningKey(key_pair, BPKUR::CHROME_BROWSER_TPM_KEY);
}

// Tests that the SigningKeyPair::Create factory function returns a properly
// initialized crypto::ECPrivateKey-backed SigningKeyPair if that is what was
// available.
TEST(SigningKeyPairTest, Create_WithECPrivateKey) {
  ECSigningKeyProvider ec_key_provider;
  auto acceptable_algorithms = {crypto::SignatureVerifier::ECDSA_SHA256};
  auto key = ec_key_provider.GenerateSigningKeySlowly(acceptable_algorithms);

  testing::StrictMock<test::MockKeyPersistenceDelegate>
      mock_persistence_delegate;
  EXPECT_CALL(mock_persistence_delegate, LoadKeyPair)
      .WillOnce(Return(KeyPersistenceDelegate::KeyInfo(
          BPKUR::CHROME_BROWSER_OS_KEY, key->GetWrappedKey())));

  absl::optional<SigningKeyPair> key_pair =
      SigningKeyPair::Create(&mock_persistence_delegate);

  ValidateSigningKey(key_pair, BPKUR::CHROME_BROWSER_OS_KEY);
}

}  // namespace enterprise_connectors
