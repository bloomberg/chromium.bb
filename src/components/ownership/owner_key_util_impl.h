// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OWNERSHIP_OWNER_KEY_UTIL_IMPL_H_
#define COMPONENTS_OWNERSHIP_OWNER_KEY_UTIL_IMPL_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/ownership/owner_key_util.h"
#include "components/ownership/ownership_export.h"

namespace ownership {

// Implementation of OwnerKeyUtil which imports public part of the
// owner key from a filesystem.
class OWNERSHIP_EXPORT OwnerKeyUtilImpl : public OwnerKeyUtil {
 public:
  explicit OwnerKeyUtilImpl(const base::FilePath& public_key_file);

  // OwnerKeyUtil implementation:
  bool ImportPublicKey(std::vector<uint8_t>* output) override;
  crypto::ScopedSECKEYPrivateKey FindPrivateKeyInSlot(
      const std::vector<uint8_t>& key,
      PK11SlotInfo* slot) override;
  bool IsPublicKeyPresent() override;

 private:
  ~OwnerKeyUtilImpl() override;

  // The file that holds the public key.
  base::FilePath public_key_file_;

  DISALLOW_COPY_AND_ASSIGN(OwnerKeyUtilImpl);
};

}  // namespace ownership

#endif  // COMPONENTS_OWNERSHIP_OWNER_KEY_UTIL_IMPL_H_
