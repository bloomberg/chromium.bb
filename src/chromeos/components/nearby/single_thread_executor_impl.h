// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_SINGLE_THREAD_EXECUTOR_IMPL_H_
#define CHROMEOS_COMPONENTS_NEARBY_SINGLE_THREAD_EXECUTOR_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/components/nearby/library/callable.h"
#include "chromeos/components/nearby/library/future.h"
#include "chromeos/components/nearby/library/runnable.h"
#include "chromeos/components/nearby/library/single_thread_executor.h"
#include "chromeos/components/nearby/submittable_executor_base.h"

namespace chromeos {

namespace nearby {

// TODO(kyleqian): Use Ptr once the Nearby library is merged in.
// Concrete location::nearby::SingleThreadExecutor implementation.
class SingleThreadExecutorImpl
    : public location::nearby::SingleThreadExecutor<SingleThreadExecutorImpl>,
      public SubmittableExecutorBase {
 public:
  SingleThreadExecutorImpl();
  ~SingleThreadExecutorImpl() override;

  // location::nearby::SubmittableExecutor:
  template <typename T>
  std::shared_ptr<location::nearby::Future<T>> submit(
      std::shared_ptr<location::nearby::Callable<T>> callable) {
    return Submit(callable);
  }

 private:
  // location::nearby::Executor:
  void shutdown() override;

  // location::nearby::SubmittableExecutor:
  void execute(std::shared_ptr<location::nearby::Runnable> runnable) override;

  DISALLOW_COPY_AND_ASSIGN(SingleThreadExecutorImpl);
};

}  // namespace nearby

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NEARBY_SINGLE_THREAD_EXECUTOR_IMPL_H_
