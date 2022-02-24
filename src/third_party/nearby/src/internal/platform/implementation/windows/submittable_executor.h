// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PLATFORM_IMPL_WINDOWS_SUBMITTABLE_EXECUTOR_H_
#define PLATFORM_IMPL_WINDOWS_SUBMITTABLE_EXECUTOR_H_

#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/implementation/windows/executor.h"

namespace location {
namespace nearby {
namespace windows {

// Main interface to be used by platform as a base class for
// - MultiThreadExecutorWrapper
// - SingleThreadExecutorWrapper
// Platform must override bool submit(std::function<void()>) method.
class SubmittableExecutor : public api::SubmittableExecutor {
 public:
  SubmittableExecutor();
  SubmittableExecutor(int32_t maxConcurrancy);
  ~SubmittableExecutor() override = default;

  // Submit a callable (with no delay).
  // Returns true, if callable was submitted, false otherwise.
  // Callable is not submitted if shutdown is in progress.
  bool DoSubmit(Runnable&& wrapped_callable) override;

  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executor.html#execute-java.lang.Runnable-
  void Execute(Runnable&& runnable) override;

  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ExecutorService.html#shutdown--
  void Shutdown() override;

 private:
  std::unique_ptr<nearby::windows::Executor> executor_;
  std::atomic_bool shut_down_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_SUBMITTABLE_EXECUTOR_H_
