// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/random.h"

#include <type_traits>

#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/rand_util.h"

namespace partition_alloc {

class RandomGenerator {
 public:
  constexpr RandomGenerator() {}

  uint32_t RandomValue() {
    ::partition_alloc::internal::ScopedGuard guard(lock_);
    return GetGenerator()->RandUint32();
  }

  void SeedForTesting(uint64_t seed) {
    ::partition_alloc::internal::ScopedGuard guard(lock_);
    GetGenerator()->ReseedForTesting(seed);
  }

 private:
  ::partition_alloc::internal::Lock lock_ = {};
  bool initialized_ GUARDED_BY(lock_) = false;
  union {
    base::InsecureRandomGenerator instance_ GUARDED_BY(lock_);
    uint8_t instance_buffer_[sizeof(base::InsecureRandomGenerator)] GUARDED_BY(
        lock_) = {};
  };

  base::InsecureRandomGenerator* GetGenerator()
      EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    if (!initialized_) {
      new (instance_buffer_) base::InsecureRandomGenerator();
      initialized_ = true;
    }
    return &instance_;
  }
};

// Note: this is redundant, since the anonymous union is incompatible with a
// non-trivial default destructor. Not meant to be destructed anyway.
static_assert(std::is_trivially_destructible<RandomGenerator>::value, "");

namespace {

RandomGenerator g_generator = {};

}  // namespace

namespace internal {

uint32_t RandomValue() {
  return g_generator.RandomValue();
}

}  // namespace internal

void SetMmapSeedForTesting(uint64_t seed) {
  return g_generator.SeedForTesting(seed);
}

}  // namespace partition_alloc
