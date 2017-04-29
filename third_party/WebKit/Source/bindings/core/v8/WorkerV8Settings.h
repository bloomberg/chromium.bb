// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerV8Settings_h
#define WorkerV8Settings_h

#include "bindings/core/v8/V8CacheOptions.h"
#include "core/CoreExport.h"

namespace blink {

// The V8 settings that are passed from the main isolate to the worker isolate.
struct CORE_EXPORT WorkerV8Settings {
  enum class HeapLimitMode { kDefault, kIncreasedForDebugging };
  enum class AtomicsWaitMode { kDisallow, kAllow };
  WorkerV8Settings(HeapLimitMode heap_limit_mode,
                   V8CacheOptions v8_cache_options,
                   AtomicsWaitMode atomics_wait_mode)
      : heap_limit_mode_(heap_limit_mode),
        v8_cache_options_(v8_cache_options),
        atomics_wait_mode_(atomics_wait_mode) {}
  static WorkerV8Settings Default() {
    return WorkerV8Settings(HeapLimitMode::kDefault, kV8CacheOptionsDefault,
                            AtomicsWaitMode::kDisallow);
  }
  HeapLimitMode heap_limit_mode_;
  V8CacheOptions v8_cache_options_;
  AtomicsWaitMode atomics_wait_mode_;
};

}  // namespace blink
#endif  // WorkerV8Setttings
