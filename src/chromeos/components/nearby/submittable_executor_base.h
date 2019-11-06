// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_SUBMITTABLE_EXECUTOR_BASE_H_
#define CHROMEOS_COMPONENTS_NEARBY_SUBMITTABLE_EXECUTOR_BASE_H_

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "chromeos/components/nearby/library/atomic_boolean.h"
#include "chromeos/components/nearby/library/callable.h"
#include "chromeos/components/nearby/library/exception.h"
#include "chromeos/components/nearby/library/future.h"
#include "chromeos/components/nearby/library/runnable.h"
#include "chromeos/components/nearby/settable_future_impl.h"

namespace chromeos {

namespace nearby {

// TODO(kyleqian): Use Ptr once the Nearby library is merged in.
class SubmittableExecutorBase {
 public:
  explicit SubmittableExecutorBase(scoped_refptr<base::TaskRunner> task_runner);
  virtual ~SubmittableExecutorBase();

 protected:
  // Once called, this method will prevent any future calls to Submit() or
  // Execute() from posting additional tasks. Previously posted asks will be
  // allowed to complete normally.
  void Shutdown();

  // Posts the given |callable| and immediately returns a Future expected to
  // eventually contain the value returned from executing |callable|. If
  // Shutdown() has been called, a Future containing
  // location::nearby::Exception::INTERRUPTED will be immediately returned. If
  // |callable| for some reason definitely will not be run sometime in the
  // future, a Future containing location::nearby::Exception::EXECUTION will be
  // immediately returned.
  template <typename T>
  std::shared_ptr<location::nearby::Future<T>> Submit(
      std::shared_ptr<location::nearby::Callable<T>> callable) {
    std::shared_ptr<SettableFutureImpl<T>> returned_future =
        std::make_shared<SettableFutureImpl<T>>();

    if (is_shut_down_->get()) {
      returned_future->SetExceptionOr(location::nearby::ExceptionOr<T>(
          location::nearby::Exception::INTERRUPTED));
      return returned_future;
    }

    if (!task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&SubmittableExecutorBase::RunTaskWithReply<T>,
                           base::Unretained(this), callable,
                           returned_future))) {
      returned_future->SetExceptionOr(location::nearby::ExceptionOr<T>(
          location::nearby::Exception::EXECUTION));
    }

    return returned_future;
  }

  // Posts the given |runnable| and returns immediately. If Shutdown() has been
  // called, this method will do nothing.
  void Execute(std::shared_ptr<location::nearby::Runnable> runnable);

 private:
  // Directly calls run() on |runnable|. This is only meant to be posted as an
  // asynchronous task within Execute().
  void RunTask(std::shared_ptr<location::nearby::Runnable> runnable);

  // Directly calls call() on |callable| and stores the returned value in
  // |previously_returned_future|, which was previously returned by a call to
  // Submit(). This is only meant to be posted as an asynchronous task within
  // Submit().
  template <typename T>
  void RunTaskWithReply(
      std::shared_ptr<location::nearby::Callable<T>> callable,
      std::shared_ptr<SettableFutureImpl<T>> previously_returned_future) {
    previously_returned_future->SetExceptionOr(callable->call());
  }

  std::unique_ptr<location::nearby::AtomicBoolean> is_shut_down_;
  scoped_refptr<base::TaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SubmittableExecutorBase);
};

}  // namespace nearby

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NEARBY_SUBMITTABLE_EXECUTOR_BASE_H_
