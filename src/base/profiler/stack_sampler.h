// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_STACK_SAMPLER_H_
#define BASE_PROFILER_STACK_SAMPLER_H_

#include <memory>
#include <vector>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/threading/platform_thread.h"

namespace base {

class Unwinder;
class ModuleCache;
class ProfileBuilder;
class StackBuffer;
class StackSamplerTestDelegate;

// StackSampler is an implementation detail of StackSamplingProfiler. It
// abstracts the native implementation required to record a set of stack frames
// for a given thread.
class BASE_EXPORT StackSampler {
 public:
  // Factory for generating a set of Unwinders for use by the profiler.
  using UnwindersFactory =
      OnceCallback<std::vector<std::unique_ptr<Unwinder>>()>;

  StackSampler(const StackSampler&) = delete;
  StackSampler& operator=(const StackSampler&) = delete;

  virtual ~StackSampler();

  // Creates a stack sampler that records samples for thread with
  // |thread_token|. Unwinders in |unwinders| must be stored in increasing
  // priority to guide unwind attempts. Only the unwinder with the lowest
  // priority is allowed to return with UnwindResult::COMPLETED. Returns null if
  // this platform does not support stack sampling.
  static std::unique_ptr<StackSampler> Create(
      SamplingProfilerThreadToken thread_token,
      ModuleCache* module_cache,
      UnwindersFactory core_unwinders_factory,
      RepeatingClosure record_sample_callback,
      StackSamplerTestDelegate* test_delegate);

  // Gets the required size of the stack buffer.
  static size_t GetStackBufferSize();

  // Creates an instance of the a stack buffer that can be used for calls to
  // any StackSampler object.
  static std::unique_ptr<StackBuffer> CreateStackBuffer();

  // The following functions are all called on the SamplingThread (not the
  // thread being sampled).

  // Performs post-construction initialization on the SamplingThread.
  virtual void Initialize() {}

  // Adds an auxiliary unwinder to handle additional, non-native-code unwind
  // scenarios. Unwinders must be inserted in increasing priority, following
  // |unwinders| provided in Create(), to guide unwind attempts.
  virtual void AddAuxUnwinder(std::unique_ptr<Unwinder> unwinder) = 0;

  // Records a set of frames and returns them.
  virtual void RecordStackFrames(StackBuffer* stackbuffer,
                                 ProfileBuilder* profile_builder) = 0;

 protected:
  StackSampler();
};

// StackSamplerTestDelegate provides seams for test code to execute during stack
// collection.
class BASE_EXPORT StackSamplerTestDelegate {
 public:
  StackSamplerTestDelegate(const StackSamplerTestDelegate&) = delete;
  StackSamplerTestDelegate& operator=(const StackSamplerTestDelegate&) = delete;

  virtual ~StackSamplerTestDelegate();

  // Called after copying the stack and resuming the target thread, but prior to
  // walking the stack. Invoked on the SamplingThread.
  virtual void OnPreStackWalk() = 0;

 protected:
  StackSamplerTestDelegate();
};

}  // namespace base

#endif  // BASE_PROFILER_STACK_SAMPLER_H_
