// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_CRASH_KEY_H_
#define COMPONENTS_GWP_ASAN_CLIENT_CRASH_KEY_H_

#include "components/gwp_asan/common/allocator_state.h"

namespace gwp_asan {
namespace internal {

// Registers a crash key that both signals to the crash handler that GWP-ASan
// has been enabled and also where to find the AllocatorState for this process.
void RegisterAllocatorAddress(uintptr_t ptr);

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CLIENT_CRASH_KEY_H_
