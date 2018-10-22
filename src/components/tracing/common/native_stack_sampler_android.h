// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_NATIVE_STACK_SAMPLER_ANDROID_H_
#define COMPONENTS_TRACING_COMMON_NATIVE_STACK_SAMPLER_ANDROID_H_

#include "base/profiler/native_stack_sampler.h"
#include "base/threading/platform_thread.h"

namespace tracing {

class StackUnwinderAndroid;

// On Android the sampling implementation is delegated and this class just
// stores a callback to the real implementation.
class NativeStackSamplerAndroid : public base::NativeStackSampler {
 public:
  // StackUnwinderAndroid supports sampling only one thread per process. So, the
  // client should ensure that no other code is using the unwinder in the
  // process. The caller must also ensure that |unwinder| is initialized for
  // sampling.
  NativeStackSamplerAndroid(base::PlatformThreadId thread_id,
                            const StackUnwinderAndroid* unwinder);
  ~NativeStackSamplerAndroid() override;

  // StackSamplingProfiler::NativeStackSampler:
  void ProfileRecordingStarting() override;
  std::vector<base::StackSamplingProfiler::Frame> RecordStackFrames(
      StackBuffer* stack_buffer,
      base::StackSamplingProfiler::ProfileBuilder* profile_builder) override;

 private:
  base::PlatformThreadId tid_;
  const StackUnwinderAndroid* unwinder_;

  DISALLOW_COPY_AND_ASSIGN(NativeStackSamplerAndroid);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_NATIVE_STACK_SAMPLER_ANDROID_H_
