// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/submittable_executor_base.h"

#include "chromeos/components/nearby/atomic_boolean_impl.h"

namespace chromeos {

namespace nearby {

SubmittableExecutorBase::SubmittableExecutorBase(
    scoped_refptr<base::TaskRunner> task_runner)
    : is_shut_down_(std::make_unique<AtomicBooleanImpl>()),
      task_runner_(task_runner) {
  is_shut_down_->set(false);
}

SubmittableExecutorBase::~SubmittableExecutorBase() = default;

void SubmittableExecutorBase::Shutdown() {
  if (!is_shut_down_->get())
    is_shut_down_->set(true);
}

void SubmittableExecutorBase::Execute(
    std::shared_ptr<location::nearby::Runnable> runnable) {
  if (is_shut_down_->get())
    return;

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&SubmittableExecutorBase::RunTask,
                                        base::Unretained(this), runnable));
}

void SubmittableExecutorBase::RunTask(
    std::shared_ptr<location::nearby::Runnable> runnable) {
  runnable->run();
}

}  // namespace nearby

}  // namespace chromeos
