// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_NATIVE_STACK_SAMPLER_ANDROID_H_
#define COMPONENTS_TRACING_COMMON_NATIVE_STACK_SAMPLER_ANDROID_H_

#include "base/profiler/native_stack_sampler.h"
#include "base/threading/platform_thread.h"
#include "components/tracing/common/stack_unwinder_android.h"

namespace tracing {

// On Android the sampling implementation is delegated and this class just
// stores a callback to the real implementation.
class NativeStackSamplerAndroid : public base::NativeStackSampler {
 public:
  // StackUnwinderAndroid only supports sampling one thread at a time. So, the
  // clients of this class must ensure synchronization between multiple
  // instances of the sampler.
  NativeStackSamplerAndroid(base::PlatformThreadId thread_id);
  ~NativeStackSamplerAndroid() override;

  // StackSamplingProfiler::NativeStackSampler:
  void ProfileRecordingStarting() override;
  std::vector<base::StackSamplingProfiler::Frame> RecordStackFrames(
      StackBuffer* stack_buffer,
      base::StackSamplingProfiler::ProfileBuilder* profile_builder) override;

 private:
  base::PlatformThreadId tid_;
  StackUnwinderAndroid unwinder_;

  DISALLOW_COPY_AND_ASSIGN(NativeStackSamplerAndroid);
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_NATIVE_STACK_SAMPLER_ANDROID_H_
