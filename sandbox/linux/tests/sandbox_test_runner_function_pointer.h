// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_TESTS_SANDBOX_TEST_RUNNER_FUNCTION_POINTER_H_
#define SANDBOX_LINUX_TESTS_SANDBOX_TEST_RUNNER_FUNCTION_POINTER_H_

#include "base/macros.h"
#include "sandbox/linux/tests/sandbox_test_runner.h"

namespace sandbox {

class SandboxTestRunnerFunctionPointer : public SandboxTestRunner {
 public:
  SandboxTestRunnerFunctionPointer(void (*function_to_run)(void));
  ~SandboxTestRunnerFunctionPointer() override;
  void Run() override;

 private:
  void (*function_to_run_)(void);
  DISALLOW_COPY_AND_ASSIGN(SandboxTestRunnerFunctionPointer);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_TESTS_SANDBOX_TEST_RUNNER__FUNCTION_POINTER_H_
