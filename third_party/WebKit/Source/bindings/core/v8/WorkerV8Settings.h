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
  enum class HeapLimitMode { Default, IncreasedForDebugging };
  WorkerV8Settings(HeapLimitMode heapLimitMode, V8CacheOptions v8CacheOptions)
      : m_heapLimitMode(heapLimitMode), m_v8CacheOptions(v8CacheOptions) {}
  static WorkerV8Settings Default() {
    return WorkerV8Settings(HeapLimitMode::Default, V8CacheOptionsDefault);
  }
  HeapLimitMode m_heapLimitMode;
  V8CacheOptions m_v8CacheOptions;
};

}  // namespace blink
#endif  // WorkerV8Setttings
