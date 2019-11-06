// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OWNERSHIP_MOCK_OWNER_KEY_UTIL_H_
#define COMPONENTS_OWNERSHIP_MOCK_OWNER_KEY_UTIL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/ownership/owner_key_util.h"
#include "components/ownership/ownership_export.h"

namespace crypto {
class RSAPrivateKey;
}

namespace ownership {

// Implementation of OwnerKeyUtil which should be used only for
// testing.
class OWNERSHIP_EXPORT MockOwnerKeyUtil : public OwnerKeyUtil {
 public:
  MockOwnerKeyUtil();

  // OwnerKeyUtil implementation:
  bool ImportPublicKey(std::vector<uint8_t>* output) override;
  crypto::ScopedSECKEYPrivateKey FindPrivateKeyInSlot(
      const std::vector<uint8_t>& key,
      PK11SlotInfo* slot) override;
  bool IsPublicKeyPresent() override;

  // Clears the public and private keys.
  void Clear();

  // Configures the mock to return the given public key.
  void SetPublicKey(const std::vector<uint8_t>& key);

  // Sets the public key to use from the given private key, but doesn't
  // configure the private key.
  void SetPublicKeyFromPrivateKey(const crypto::RSAPrivateKey& key);

  // Sets the private key (also configures the public key).
  void SetPrivateKey(std::unique_ptr<crypto::RSAPrivateKey> key);

 private:
  ~MockOwnerKeyUtil() override;

  std::vector<uint8_t> public_key_;
  crypto::ScopedSECKEYPrivateKey private_key_;

  DISALLOW_COPY_AND_ASSIGN(MockOwnerKeyUtil);
};

}  // namespace ownership

#endif  // COMPONENTS_OWNERSHIP_MOCK_OWNER_KEY_UTIL_H_
