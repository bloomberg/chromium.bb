// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampler.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/profiler/native_unwinder_win.h"
#include "base/profiler/stack_copier_suspend.h"
#include "base/profiler/stack_sampler_impl.h"
#include "base/profiler/suspendable_thread_delegate_win.h"
#include "build/build_config.h"

namespace base {

// static
std::unique_ptr<StackSampler> StackSampler::Create(
    SamplingProfilerThreadToken thread_token,
    ModuleCache* module_cache,
    UnwindersFactory core_unwinders_factory,
    RepeatingClosure record_sample_callback,
    StackSamplerTestDelegate* test_delegate) {
  DCHECK(!core_unwinders_factory);
#if defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64)
  const auto create_unwinders = []() {
    std::vector<std::unique_ptr<Unwinder>> unwinders;
    unwinders.push_back(std::make_unique<NativeUnwinderWin>());
    return unwinders;
  };
  return std::make_unique<StackSamplerImpl>(
      std::make_unique<StackCopierSuspend>(
          std::make_unique<SuspendableThreadDelegateWin>(thread_token)),
      BindOnce(create_unwinders), module_cache,
      std::move(record_sample_callback), test_delegate);
#else
  return nullptr;
#endif
}

// static
size_t StackSampler::GetStackBufferSize() {
  // The default Win32 reserved stack size is 1 MB and Chrome Windows threads
  // currently always use the default, but this allows for expansion if it
  // occurs. The size beyond the actual stack size consists of unallocated
  // virtual memory pages so carries little cost (just a bit of wasted address
  // space).
  return 2 << 20;  // 2 MiB
}

}  // namespace base
