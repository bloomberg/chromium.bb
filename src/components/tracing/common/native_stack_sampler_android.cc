// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/native_stack_sampler_android.h"

#include "base/trace_event/trace_event.h"

namespace tracing {
namespace {
constexpr size_t kMaxFrameDepth = 48;
}  // namespace

NativeStackSamplerAndroid::NativeStackSamplerAndroid(base::PlatformThreadId tid)
    : tid_(tid) {}

NativeStackSamplerAndroid::~NativeStackSamplerAndroid() = default;

void NativeStackSamplerAndroid::ProfileRecordingStarting() {}

std::vector<base::StackSamplingProfiler::Frame>
NativeStackSamplerAndroid::RecordStackFrames(
    StackBuffer* stack_buffer,
    base::StackSamplingProfiler::ProfileBuilder* profile_builder) {
  if (!unwinder_.is_initialized()) {
    // May block on disk access. This function is executed on the profiler
    // thread, so this will only block profiling execution.
    TRACE_EVENT0("cpu_profiler", "StackUnwinderAndroid::Initialize");
    unwinder_.Initialize();
  }
  const void* pcs[kMaxFrameDepth];
  size_t depth = unwinder_.TraceStack(tid_, pcs, kMaxFrameDepth);
  std::vector<base::StackSamplingProfiler::Frame> frames;
  frames.reserve(depth);
  for (size_t i = 0; i < depth; ++i) {
    // TODO(ssid): Add support for obtaining modules here.
    frames.emplace_back(reinterpret_cast<uintptr_t>(pcs[i]),
                        base::ModuleCache::Module());
  }
  return frames;
}

}  // namespace tracing
