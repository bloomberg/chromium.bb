// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file is a no-op if the required LowLevelAlloc support is missing.
#include "absl/base/internal/low_level_alloc.h"
#ifndef ABSL_LOW_LEVEL_ALLOC_MISSING

#include "absl/synchronization/internal/per_thread_sem.h"

#include <atomic>

#include "absl/base/attributes.h"
#include "absl/base/internal/malloc_extension.h"
#include "absl/base/internal/thread_identity.h"
#include "absl/synchronization/internal/waiter.h"

namespace absl {
namespace synchronization_internal {

void PerThreadSem::SetThreadBlockedCounter(std::atomic<int> *counter) {
  base_internal::ThreadIdentity *identity;
  identity = GetOrCreateCurrentThreadIdentity();
  identity->blocked_count_ptr = counter;
}

std::atomic<int> *PerThreadSem::GetThreadBlockedCounter() {
  base_internal::ThreadIdentity *identity;
  identity = GetOrCreateCurrentThreadIdentity();
  return identity->blocked_count_ptr;
}

void PerThreadSem::Init(base_internal::ThreadIdentity *identity) {
  Waiter::GetWaiter(identity)->Init();
  identity->ticker.store(0, std::memory_order_relaxed);
  identity->wait_start.store(0, std::memory_order_relaxed);
  identity->is_idle.store(false, std::memory_order_relaxed);
}

void PerThreadSem::Tick(base_internal::ThreadIdentity *identity) {
  const int ticker =
      identity->ticker.fetch_add(1, std::memory_order_relaxed) + 1;
  const int wait_start = identity->wait_start.load(std::memory_order_relaxed);
  const bool is_idle = identity->is_idle.load(std::memory_order_relaxed);
  if (wait_start && (ticker - wait_start > Waiter::kIdlePeriods) && !is_idle) {
    // Wakeup the waiting thread since it is time for it to become idle.
    Waiter::GetWaiter(identity)->Poke();
  }
}

}  // namespace synchronization_internal
}  // namespace absl

extern "C" {

ABSL_ATTRIBUTE_WEAK void AbslInternalPerThreadSemPost(
    absl::base_internal::ThreadIdentity *identity) {
  absl::synchronization_internal::Waiter::GetWaiter(identity)->Post();
}

ABSL_ATTRIBUTE_WEAK bool AbslInternalPerThreadSemWait(
    absl::synchronization_internal::KernelTimeout t) {
  bool timeout = false;
  absl::base_internal::ThreadIdentity *identity;
  identity = absl::synchronization_internal::GetOrCreateCurrentThreadIdentity();

  // Ensure wait_start != 0.
  int ticker = identity->ticker.load(std::memory_order_relaxed);
  identity->wait_start.store(ticker ? ticker : 1, std::memory_order_relaxed);
  identity->is_idle.store(false, std::memory_order_relaxed);

  if (identity->blocked_count_ptr != nullptr) {
    // Increment count of threads blocked in a given thread pool.
    identity->blocked_count_ptr->fetch_add(1, std::memory_order_relaxed);
  }

  timeout =
      !absl::synchronization_internal::Waiter::GetWaiter(identity)->Wait(t);

  if (identity->blocked_count_ptr != nullptr) {
    identity->blocked_count_ptr->fetch_sub(1, std::memory_order_relaxed);
  }

  if (identity->is_idle.load(std::memory_order_relaxed)) {
    // We became idle during the wait; become non-idle again so that
    // performance of deallocations done from now on does not suffer.
    absl::base_internal::MallocExtension::instance()->MarkThreadBusy();
  }
  identity->is_idle.store(false, std::memory_order_relaxed);
  identity->wait_start.store(0, std::memory_order_relaxed);
  return !timeout;
}

}  // extern "C"

#endif  // ABSL_LOW_LEVEL_ALLOC_MISSING
