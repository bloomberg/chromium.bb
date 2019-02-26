// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_SAMPLING_PROFILER_WRAPPER_H_
#define COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_SAMPLING_PROFILER_WRAPPER_H_

#include <memory>

#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"

namespace heap_profiling {

class SamplingProfilerWrapper
    : private base::PoissonAllocationSampler::SamplesObserver {
 public:
  SamplingProfilerWrapper();
  ~SamplingProfilerWrapper() override;

  void StartProfiling(size_t sampling_rate);

 private:
  // base::PoissonAllocationSampler::SamplesObserver
  void SampleAdded(void* address,
                   size_t size,
                   size_t total,
                   base::PoissonAllocationSampler::AllocatorType,
                   const char* context) override;
  void SampleRemoved(void* address) override;
};

}  // namespace heap_profiling

#endif  // COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_SAMPLING_PROFILER_WRAPPER_H_
