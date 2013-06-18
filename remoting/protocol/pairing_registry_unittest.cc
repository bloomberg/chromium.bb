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
  virtual void SetUp() OVERRIDE {
    got_secret_ = false;
  }

  void CompareSecret(const std::string& expected,
                     PairingRegistry::Pairing actual) {
    EXPECT_EQ(actual.shared_secret(), expected);
    got_secret_ = true;
  }

 protected:
  bool got_secret_;
};

TEST_F(PairingRegistryTest, GetPairing) {
  PairingRegistry::Pairing client_info =
      PairingRegistry::Pairing::Create("client_name");
  MockPairingRegistryDelegate* mock_delegate =
      new MockPairingRegistryDelegate();
  mock_delegate->AddPairing(client_info, PairingRegistry::AddPairingCallback());
  scoped_ptr<PairingRegistry::Delegate> delegate(mock_delegate);

  scoped_refptr<PairingRegistry> registry(new PairingRegistry(delegate.Pass()));

  registry->GetPairing(client_info.client_id(),
                       base::Bind(&PairingRegistryTest::CompareSecret,
                                  base::Unretained(this),
                                  client_info.shared_secret()));
  mock_delegate->RunCallback();
  EXPECT_TRUE(got_secret_);
}

TEST_F(PairingRegistryTest, AddPairing) {
  MockPairingRegistryDelegate* mock_delegate =
      new MockPairingRegistryDelegate();
  scoped_ptr<PairingRegistry::Delegate> delegate(mock_delegate);

  scoped_refptr<PairingRegistry> registry(new PairingRegistry(delegate.Pass()));

  // Verify that we can create pairings from two clients with the same name, but
  // that they aren't allocated the same client id.
  PairingRegistry::Pairing pairing_1 = registry->CreatePairing("client_name");
  PairingRegistry::Pairing pairing_2 = registry->CreatePairing("client_name");

  const PairingRegistry::PairedClients& clients =
      mock_delegate->paired_clients();
  ASSERT_EQ(clients.size(), 2u);
  PairingRegistry::Pairing first = clients.begin()->second;
  PairingRegistry::Pairing second = (++clients.begin())->second;
  EXPECT_EQ(first.client_name(), second.client_name());
  EXPECT_NE(first.client_id(), second.client_id());
}

}  // namespace protocol
}  // namespace remoting
