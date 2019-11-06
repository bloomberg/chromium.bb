// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/crypto/sha2.h"

#include <stddef.h>

#include <memory>

#include "util/crypto/secure_hash.h"
#include "util/std_util.h"

namespace openscreen {

void SHA256HashString(absl::string_view str,
                      uint8_t output[SHA256_DIGEST_LENGTH]) {
  SHA256(reinterpret_cast<const uint8_t*>(str.data()), str.length(), output);
}

std::string SHA256HashString(absl::string_view str) {
  std::string output(SHA256_DIGEST_LENGTH, 0);
  SHA256HashString(str, reinterpret_cast<uint8_t*>(data(output)));
  return output;
}

}  // namespace openscreen
