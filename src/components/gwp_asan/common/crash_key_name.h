// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_COMMON_CRASH_KEY_NAME_H_
#define COMPONENTS_GWP_ASAN_COMMON_CRASH_KEY_NAME_H_

namespace gwp_asan {
namespace internal {

// The name of the crash key used to convey the address of the
// AllocatorBaseState to the crash handler.
const char kGpaCrashKey[] = "allocator-base-state-address";

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_COMMON_CRASH_KEY_NAME_H_
