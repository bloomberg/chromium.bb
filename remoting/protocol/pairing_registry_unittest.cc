// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_registry.h"

#include <stdlib.h>

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using remoting::protocol::PairingRegistry;

// Verify that a pairing Dictionary has correct entries, but doesn't include
// any shared secret.
void VerifyPairing(PairingRegistry::Pairing expected,
                   const base::DictionaryValue& actual) {
  std::string value;
  EXPECT_TRUE(actual.GetString(PairingRegistry::kClientNameKey, &value));
  EXPECT_EQ(expected.client_name(), value);
  EXPECT_TRUE(actual.GetString(PairingRegistry::kClientIdKey, &value));
  EXPECT_EQ(expected.client_id(), value);

  EXPECT_FALSE(actual.HasKey(PairingRegistry::kSharedSecretKey));
}

// SaveCallback that expects to be called with success = true.
void ExpectSaveSuccess(bool success) {
  EXPECT_TRUE(success);
}

}  // namespace

namespace remoting {
namespace protocol {

class PairingRegistryTest : public testing::Test {
 public:
  void CompareSecret(const std::string& expected,
                     PairingRegistry::Pairing actual) {
    EXPECT_EQ(expected, actual.shared_secret());
    secret_ = actual.shared_secret();
    got_secret_ = true;
  }

  void set_pairings(scoped_ptr<base::ListValue> pairings) {
    pairings_ = pairings.Pass();
  }

 protected:
  std::string secret_;
  bool got_secret_;
  scoped_ptr<base::ListValue> pairings_;
};

TEST_F(PairingRegistryTest, CreateAndGetPairings) {
  MockPairingRegistryDelegate* mock_delegate =
      new MockPairingRegistryDelegate();
  scoped_ptr<PairingRegistry::Delegate> delegate(mock_delegate);

  scoped_refptr<PairingRegistry> registry(new PairingRegistry(delegate.Pass()));
  PairingRegistry::Pairing pairing_1 = registry->CreatePairing("my_client");
  mock_delegate->RunCallback();
  PairingRegistry::Pairing pairing_2 = registry->CreatePairing("my_client");
  mock_delegate->RunCallback();

  registry->GetPairing(pairing_1.client_id(),
                       base::Bind(&PairingRegistryTest::CompareSecret,
                                  base::Unretained(this),
                                  pairing_1.shared_secret()));
  got_secret_ = false;
  mock_delegate->RunCallback();
  EXPECT_TRUE(got_secret_);
  std::string secret_1 = secret_;

  // Check that the second client is paired with a different shared secret.
  registry->GetPairing(pairing_2.client_id(),
                       base::Bind(&PairingRegistryTest::CompareSecret,
                                  base::Unretained(this),
                                  pairing_2.shared_secret()));
  got_secret_ = false;
  mock_delegate->RunCallback();
  EXPECT_TRUE(got_secret_);
  EXPECT_NE(secret_, secret_1);
}

TEST_F(PairingRegistryTest, GetAllPairings) {
  MockPairingRegistryDelegate* mock_delegate =
      new MockPairingRegistryDelegate();
  scoped_ptr<PairingRegistry::Delegate> delegate(mock_delegate);

  scoped_refptr<PairingRegistry> registry(new PairingRegistry(delegate.Pass()));
  PairingRegistry::Pairing pairing_1 = registry->CreatePairing("client1");
  mock_delegate->RunCallback();
  PairingRegistry::Pairing pairing_2 = registry->CreatePairing("client2");
  mock_delegate->RunCallback();

  registry->GetAllPairings(
      base::Bind(&PairingRegistryTest::set_pairings,
                 base::Unretained(this)));
  mock_delegate->RunCallback();

  ASSERT_EQ(2u, pairings_->GetSize());
  const base::DictionaryValue* actual_pairing_1;
  const base::DictionaryValue* actual_pairing_2;
  ASSERT_TRUE(pairings_->GetDictionary(0, &actual_pairing_1));
  ASSERT_TRUE(pairings_->GetDictionary(1, &actual_pairing_2));

  // Ordering is not guaranteed, so swap if necessary.
  std::string actual_client_id;
  ASSERT_TRUE(actual_pairing_1->GetString(PairingRegistry::kClientIdKey,
                                          &actual_client_id));
  if (actual_client_id != pairing_1.client_id()) {
    std::swap(actual_pairing_1, actual_pairing_2);
  }

  VerifyPairing(pairing_1, *actual_pairing_1);
  VerifyPairing(pairing_2, *actual_pairing_2);
}

TEST_F(PairingRegistryTest, DeletePairing) {
  MockPairingRegistryDelegate* mock_delegate =
      new MockPairingRegistryDelegate();
  scoped_ptr<PairingRegistry::Delegate> delegate(mock_delegate);

  scoped_refptr<PairingRegistry> registry(new PairingRegistry(delegate.Pass()));
  PairingRegistry::Pairing pairing_1 = registry->CreatePairing("client1");
  mock_delegate->RunCallback();
  PairingRegistry::Pairing pairing_2 = registry->CreatePairing("client2");
  mock_delegate->RunCallback();

  registry->DeletePairing(pairing_1.client_id(), base::Bind(ExpectSaveSuccess));
  mock_delegate->RunCallback();

  // Re-read the list, and verify it only has the pairing_2 client.
  registry->GetAllPairings(
      base::Bind(&PairingRegistryTest::set_pairings,
                 base::Unretained(this)));
  mock_delegate->RunCallback();

  ASSERT_EQ(1u, pairings_->GetSize());
  const base::DictionaryValue* actual_pairing_2;
  ASSERT_TRUE(pairings_->GetDictionary(0, &actual_pairing_2));
  std::string actual_client_id;
  ASSERT_TRUE(actual_pairing_2->GetString(PairingRegistry::kClientIdKey,
                                          &actual_client_id));
  EXPECT_EQ(pairing_2.client_id(), actual_client_id);
}

TEST_F(PairingRegistryTest, ClearAllPairings) {
  MockPairingRegistryDelegate* mock_delegate =
      new MockPairingRegistryDelegate();
  scoped_ptr<PairingRegistry::Delegate> delegate(mock_delegate);

  scoped_refptr<PairingRegistry> registry(new PairingRegistry(delegate.Pass()));
  PairingRegistry::Pairing pairing_1 = registry->CreatePairing("client1");
  mock_delegate->RunCallback();
  PairingRegistry::Pairing pairing_2 = registry->CreatePairing("client2");
  mock_delegate->RunCallback();

  registry->ClearAllPairings(base::Bind(ExpectSaveSuccess));

  // Re-read the list, and verify it is empty.
  registry->GetAllPairings(
      base::Bind(&PairingRegistryTest::set_pairings,
                 base::Unretained(this)));
  mock_delegate->RunCallback();

  EXPECT_TRUE(pairings_->empty());
}

}  // namespace protocol
}  // namespace remoting
