// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_HASH_UTILS_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_HASH_UTILS_H_

#include <memory>
#include <string>

#include "chromeos/components/nearby/library/byte_array.h"

namespace location {

namespace nearby {

// A provider of standard hashing algorithms.
// TODO(kyleqian): Remove this file pending Nearby library import. This is a
// temporary placeholder for the HashUtils pure-virtual class from the Nearby
// library, which is a dependency of HashUtilsImpl. See bug #861813 ->
// https://crbug.com/861813.
class HashUtils {
 public:
  virtual ~HashUtils() {}

  virtual std::unique_ptr<ByteArray> md5(const std::string& input) = 0;
  virtual std::unique_ptr<ByteArray> sha256(const std::string& input) = 0;
};

}  // namespace nearby

}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_HASH_UTILS_H_
