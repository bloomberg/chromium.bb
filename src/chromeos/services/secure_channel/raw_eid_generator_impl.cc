// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/raw_eid_generator_impl.h"

#include "base/sys_byteorder.h"
#include "crypto/sha2.h"

namespace chromeos {

namespace secure_channel {

const int32_t RawEidGenerator::kNumBytesInEidValue = 2;

RawEidGeneratorImpl::RawEidGeneratorImpl() {}

RawEidGeneratorImpl::~RawEidGeneratorImpl() {}

std::string RawEidGeneratorImpl::GenerateEid(
    const std::string& eid_seed,
    int64_t start_of_period_timestamp_ms,
    std::string const* extra_entropy) {
  // The data to hash is the eid seed, followed by the extra entropy (if it
  // exists), followed by the timestamp.
  std::string to_hash = eid_seed;
  if (extra_entropy) {
    to_hash += *extra_entropy;
  }
  uint64_t timestamp_data =
      base::HostToNet64(static_cast<uint64_t>(start_of_period_timestamp_ms));
  to_hash.append(reinterpret_cast<char*>(&timestamp_data), sizeof(uint64_t));

  std::string result = crypto::SHA256HashString(to_hash);
  result.resize(RawEidGenerator::kNumBytesInEidValue);
  return result;
}

}  // namespace secure_channel

}  // namespace chromeos
