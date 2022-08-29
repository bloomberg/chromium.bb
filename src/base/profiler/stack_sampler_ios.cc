// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampler.h"

#include "base/profiler/profiler_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(IOS_STACK_PROFILER_ENABLED)
#include "base/bind.h"
#include "base/check.h"
#include "base/profiler/native_unwinder_apple.h"
#include "base/profiler/stack_copier_suspend.h"
#include "base/profiler/stack_sampler_impl.h"
#include "base/profiler/suspendable_thread_delegate_mac.h"
#endif

namespace base {

#if BUILDFLAG(IOS_STACK_PROFILER_ENABLED)
namespace {

std::vector<std::unique_ptr<Unwinder>> CreateUnwinders() {
  std::vector<std::unique_ptr<Unwinder>> unwinders;
  if (__builtin_available(iOS 12.0, *)) {
    unwinders.push_back(std::make_unique<NativeUnwinderApple>());
  }
  return unwinders;
}

}  // namespace
#endif

// static
std::unique_ptr<StackSampler> StackSampler::Create(
    SamplingProfilerThreadToken thread_token,
    ModuleCache* module_cache,
    UnwindersFactory core_unwinders_factory,
    RepeatingClosure record_sample_callback,
    StackSamplerTestDelegate* test_delegate) {
  DCHECK(!core_unwinders_factory);
#if BUILDFLAG(IOS_STACK_PROFILER_ENABLED)
  return std::make_unique<StackSamplerImpl>(
      std::make_unique<StackCopierSuspend>(
          std::make_unique<SuspendableThreadDelegateMac>(thread_token)),
      BindOnce(&CreateUnwinders), module_cache,
      std::move(record_sample_callback), test_delegate);
#else
  return nullptr;
#endif
}

// static
size_t StackSampler::GetStackBufferSize() {
#if BUILDFLAG(IOS_STACK_PROFILER_ENABLED)
  size_t stack_size = PlatformThread::GetDefaultThreadStackSize();

  // If getrlimit somehow fails, return the default iOS main thread stack size
  // of 1 MB with extra wiggle room.
  return stack_size > 0 ? stack_size : 1536 * 1024;
#else
  return 0;
#endif
}

}  // namespace base
