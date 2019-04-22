// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/sandbox_request_helper.h"

#include <utility>

namespace chrome_cleaner {

// static
MojoCallStatus MojoCallStatus::Success() {
  return MojoCallStatus{MOJO_CALL_MADE};
}

// static
MojoCallStatus MojoCallStatus::Failure(SandboxErrorCode error_code) {
  return MojoCallStatus{MOJO_CALL_ERROR, error_code};
}

namespace internal {

void SaveMojoCallStatus(base::OnceClosure quit_closure,
                        MojoCallStatus* status_out,
                        MojoCallStatus status) {
  *status_out = status;

  // Only call the quit closure if there was an error, since then there won't be
  // other callbacks.
  if (status.state != MojoCallStatus::MOJO_CALL_MADE)
    std::move(quit_closure).Run();
}

// Returns a wrapper that executes |closure| on the given |task_runner|.
base::OnceClosure ClosureForTaskRunner(
    scoped_refptr<base::TaskRunner> task_runner,
    base::OnceClosure closure) {
  return base::BindOnce(
      [](scoped_refptr<base::TaskRunner> task_runner,
         base::OnceClosure closure) {
        task_runner->PostTask(FROM_HERE, std::move(closure));
      },
      task_runner, std::move(closure));
}

}  // namespace internal

}  // namespace chrome_cleaner
