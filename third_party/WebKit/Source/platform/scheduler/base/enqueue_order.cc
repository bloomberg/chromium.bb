// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/enqueue_order.h"

namespace blink {
namespace scheduler {
namespace internal {

// Note we set the first |enqueue_order_| to 1 so that isValid may distinguish a
// valid EnqueueOrder from an invalid one.
EnqueueOrderGenerator::EnqueueOrderGenerator() : enqueue_order_(1) {}

EnqueueOrderGenerator::~EnqueueOrderGenerator() {}

EnqueueOrder EnqueueOrderGenerator::GenerateNext() {
  base::AutoLock lock(lock_);
  return enqueue_order_++;
}

}  // namespace internal
}  // namespace scheduler
}  // namespace blink
