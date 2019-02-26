// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "components/gwp_asan/client/crash_key.h"

#include "base/debug/crash_logging.h"
#include "base/strings/stringprintf.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/common/crash_key_name.h"

namespace gwp_asan {
namespace internal {

void RegisterAllocatorAddress(uintptr_t ptr) {
  static crash_reporter::CrashKeyString<24> gpa_crash_key(kGpaCrashKey);
  gpa_crash_key.Set(base::StringPrintf("%zx", ptr));
}

}  // namespace internal
}  // namespace gwp_asan
