// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_IPC_FUZZER_MUTATE_RAND_UTIL_H_
#define TOOLS_IPC_FUZZER_MUTATE_RAND_UTIL_H_

#include "base/basictypes.h"
#include "third_party/mt19937ar/mt19937ar.h"

namespace ipc_fuzzer {

extern MersenneTwister* g_mersenne_twister;

void InitRand();

inline uint32 RandU32() {
  return g_mersenne_twister->genrand_int32();
}

inline uint64 RandU64() {
  return (static_cast<uint64>(RandU32()) << 32) | RandU32();
}

inline double RandDouble() {
  uint64 rand_u64 = RandU64();
  return *reinterpret_cast<double*>(&rand_u64);
}

inline uint32 RandInRange(uint32 range) {
  return RandU32() % range;
}

inline bool RandEvent(uint32 frequency) {
  return RandInRange(frequency) == 0;
}

}  // namespace ipc_fuzzer

#endif  // TOOLS_IPC_FUZZER_MUTATE_RAND_UTIL_H_
