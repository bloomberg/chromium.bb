// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerBackingThreadStartupData_h
#define WorkerBackingThreadStartupData_h

#include "platform/CrossThreadCopier.h"
#include "platform/wtf/Optional.h"

namespace blink {

// WorkerBackingThreadStartupData contains parameters for starting
// WorkerBackingThread.
struct WorkerBackingThreadStartupData {
 public:
  enum class HeapLimitMode { kDefault, kIncreasedForDebugging };
  enum class AtomicsWaitMode { kDisallow, kAllow };

  WorkerBackingThreadStartupData(HeapLimitMode heap_limit_mode,
                                 AtomicsWaitMode atomics_wait_mode)
      : heap_limit_mode(heap_limit_mode),
        atomics_wait_mode(atomics_wait_mode) {}

  static WorkerBackingThreadStartupData CreateDefault() {
    return WorkerBackingThreadStartupData(HeapLimitMode::kDefault,
                                          AtomicsWaitMode::kDisallow);
  }

  HeapLimitMode heap_limit_mode;
  AtomicsWaitMode atomics_wait_mode;
};

// This allows to pass WTF::Optional<WorkerBackingThreadStartupData> across
// threads by PostTask().
template <>
struct CrossThreadCopier<WTF::Optional<WorkerBackingThreadStartupData>>
    : public CrossThreadCopierPassThrough<
          WTF::Optional<WorkerBackingThreadStartupData>> {};

}  // namespace blink

#endif  // WorkerBackingThreadStartupData_h
