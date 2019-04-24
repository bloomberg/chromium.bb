// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SINGLE_THREAD_EXECUTOR_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SINGLE_THREAD_EXECUTOR_H_

#include "chromeos/components/nearby/library/submittable_executor.h"

namespace location {
namespace nearby {

// An Executor that uses a single worker thread operating off an unbounded
// queue.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newSingleThreadExecutor--
template <typename ConcreteSubmittableExecutor>
class SingleThreadExecutor
    : public SubmittableExecutor<ConcreteSubmittableExecutor> {
 public:
  virtual ~SingleThreadExecutor() {}
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SINGLE_THREAD_EXECUTOR_H_
