// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"

#include "base/numerics/safe_conversions.h"
#include "base/unguessable_token.h"
#include "third_party/smhasher/src/MurmurHash2.h"

namespace resource_coordinator {

namespace {

// The seed to use when taking the murmur2 hash of the id.
const uint64_t kMurmur2HashSeed = 0;

uint64_t CreateMurmurHash64A(const std::string& id) {
  DCHECK(base::IsValueInRangeForNumericType<int>(id.size()));

  return MurmurHash64A(&id.front(), static_cast<int>(id.size()),
                       kMurmur2HashSeed);
}

}  // namespace

CoordinationUnitID::CoordinationUnitID()
    : id(0), type(CoordinationUnitType::kInvalidType) {}

CoordinationUnitID::CoordinationUnitID(const CoordinationUnitType& type,
                                       const std::string& new_id)
    : type(type) {
  id = CreateMurmurHash64A(
      !new_id.empty() ? new_id : base::UnguessableToken().Create().ToString());
}

CoordinationUnitID::CoordinationUnitID(const CoordinationUnitType& type,
                                       uint64_t new_id)
    : id(new_id), type(type) {}

}  // namespace resource_coordinator
