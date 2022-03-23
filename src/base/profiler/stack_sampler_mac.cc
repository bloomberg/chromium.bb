// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampler.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/profiler/native_unwinder.h"
#include "base/profiler/stack_copier_suspend.h"
#include "base/profiler/stack_sampler_impl.h"
#include "base/profiler/suspendable_thread_delegate_mac.h"

namespace base {

namespace {

std::vector<std::unique_ptr<Unwinder>> CreateUnwinders(
    ModuleCache* module_cache) {
  std::vector<std::unique_ptr<Unwinder>> unwinders;
  unwinders.push_back(CreateNativeUnwinder(module_cache));
  return unwinders;
}

}  // namespace

// static
std::unique_ptr<StackSampler> StackSampler::Create(
    SamplingProfilerThreadToken thread_token,
    ModuleCache* module_cache,
    UnwindersFactory core_unwinders_factory,
    RepeatingClosure record_sample_callback,
    StackSamplerTestDelegate* test_delegate) {
  DCHECK(!core_unwinders_factory);
  return std::make_unique<StackSamplerImpl>(
      std::make_unique<StackCopierSuspend>(
          std::make_unique<SuspendableThreadDelegateMac>(thread_token)),
      BindOnce(&CreateUnwinders, Unretained(module_cache)), module_cache,
      std::move(record_sample_callback), test_delegate);
}

// static
size_t StackSampler::GetStackBufferSize() {
  size_t stack_size = PlatformThread::GetDefaultThreadStackSize();

  // If getrlimit somehow fails, return the default macOS main thread stack size
  // of 8 MB (DFLSSIZ in <i386/vmparam.h>) with extra wiggle room.
  return stack_size > 0 ? stack_size : 12 * 1024 * 1024;
}

}  // namespace base
