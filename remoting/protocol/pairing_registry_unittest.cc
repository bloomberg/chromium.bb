// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_registry.h"

#include <stdlib.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

 protected:
  std::string secret_;
  bool got_secret_;
};

TEST_F(PairingRegistryTest, CreateAndGetPairings) {
  MockPairingRegistryDelegate* mock_delegate =
      new MockPairingRegistryDelegate();
  scoped_ptr<PairingRegistry::Delegate> delegate(mock_delegate);

  scoped_refptr<PairingRegistry> registry(new PairingRegistry(delegate.Pass()));
  PairingRegistry::Pairing pairing_1 = registry->CreatePairing("client_name");
  mock_delegate->RunCallback();
  PairingRegistry::Pairing pairing_2 = registry->CreatePairing("client_name");
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

}  // namespace protocol
}  // namespace remoting
