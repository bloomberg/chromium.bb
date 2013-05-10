// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pairing_registry.h"

#include <stdlib.h>

#include "base/compiler_specific.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

class PairingRegistryTest : public testing::Test {
};

class MockDelegate : public PairingRegistry::Delegate {
 public:
  // MockDelegate saves to an explicit external PairedClients instance because
  // PairingRegistry takes ownership of it and makes no guarantees about its
  // lifetime, so this is a simple way of getting access to pairing results.
  MockDelegate(PairingRegistry::PairedClients* paired_clients)
      : paired_clients_(paired_clients) {
  }

  virtual void Save(
      const PairingRegistry::PairedClients& paired_clients) OVERRIDE {
    *paired_clients_ = paired_clients;
  }

 protected:
  PairingRegistry::PairedClients* paired_clients_;
};

TEST_F(PairingRegistryTest, LoadAndLookup) {
  PairingRegistry::Pairing client_info = {
    "client_id",
    "client_name",
    "shared_secret"
  };
  PairingRegistry::PairedClients clients;
  clients[client_info.client_id] = client_info;
  scoped_ptr<PairingRegistry::Delegate> delegate(new MockDelegate(&clients));

  scoped_refptr<PairingRegistry> registry(new PairingRegistry(delegate.Pass(),
                                                              clients));

  std::string secret = registry->GetSecret(client_info.client_id);
  EXPECT_EQ(secret, client_info.shared_secret);
}

TEST_F(PairingRegistryTest, CreateAndSave) {
  PairingRegistry::PairedClients clients;
  scoped_ptr<PairingRegistry::Delegate> delegate(new MockDelegate(&clients));

  scoped_refptr<PairingRegistry> registry(new PairingRegistry(delegate.Pass(),
                                                              clients));

  // Verify that we can create pairings from two clients with the same name, but
  // that they aren't allocated the same client id.
  PairingRegistry::Pairing pairing_1 = registry->CreatePairing("client_name");
  PairingRegistry::Pairing pairing_2 = registry->CreatePairing("client_name");

  ASSERT_EQ(clients.size(), 2u);
  PairingRegistry::Pairing first = clients.begin()->second;
  PairingRegistry::Pairing second = (++clients.begin())->second;
  EXPECT_EQ(first.client_name, second.client_name);
  EXPECT_NE(first.client_id, second.client_id);
}

}  // namespace protocol
}  // namespace remoting
