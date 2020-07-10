// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_HASH_UTILS_IMPL_H_
#define CHROMEOS_COMPONENTS_NEARBY_HASH_UTILS_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chromeos/components/nearby/library/byte_array.h"
#include "chromeos/components/nearby/library/hash_utils.h"

namespace chromeos {

namespace nearby {

// Concrete location::nearby::HashUtils implementation.
class HashUtilsImpl : public location::nearby::HashUtils {
 public:
  HashUtilsImpl();
  ~HashUtilsImpl() override;

 private:
  // location::nearby::HashUtils:
  std::unique_ptr<location::nearby::ByteArray> md5(
      const std::string& input) override;
  std::unique_ptr<location::nearby::ByteArray> sha256(
      const std::string& input) override;

  DISALLOW_COPY_AND_ASSIGN(HashUtilsImpl);
};

}  // namespace nearby

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NEARBY_HASH_UTILS_IMPL_H_
