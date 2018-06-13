// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_ENQUEUE_ORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_ENQUEUE_ORDER_H_

#include <stdint.h>

#include <atomic>

#include "base/macros.h"

namespace base {
namespace sequence_manager {
namespace internal {

// 64-bit number which is used to order tasks.
// SequenceManager assumes this number will never overflow.
class EnqueueOrder {
 public:
  EnqueueOrder() : value_(kNone) {}
  ~EnqueueOrder() = default;

  static EnqueueOrder none() { return EnqueueOrder(kNone); }
  static EnqueueOrder blocking_fence() { return EnqueueOrder(kBlockingFence); }

  // It's okay to use EnqueueOrder in boolean expressions keeping in mind
  // that some non-zero values have a special meaning.
  operator uint64_t() const { return value_; }

  static EnqueueOrder FromIntForTesting(uint64_t value) {
    return EnqueueOrder(value);
  }

  // EnqueueOrder can't be created from a raw number in non-test code.
  // Generator is used to create it with strictly monotonic guarantee.
  class Generator {
   public:
    Generator() : counter_(kFirst) {}
    ~Generator() = default;

    // Can be called from any thread.
    EnqueueOrder GenerateNext() {
      return EnqueueOrder(std::atomic_fetch_add_explicit(
          &counter_, uint64_t(1), std::memory_order_seq_cst));
    }

   private:
    std::atomic<uint64_t> counter_;
    DISALLOW_COPY_AND_ASSIGN(Generator);
  };

 private:
  explicit EnqueueOrder(uint64_t value) : value_(value) {}

  enum SpecialValues : uint64_t {
    kNone = 0,
    kBlockingFence = 1,
    kFirst = 2,
  };

  uint64_t value_;
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_ENQUEUE_ORDER_H_
