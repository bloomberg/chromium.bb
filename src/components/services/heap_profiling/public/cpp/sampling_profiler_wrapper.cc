// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/sampling_profiler_wrapper.h"

#include "components/services/heap_profiling/public/cpp/allocator_shim.h"

namespace heap_profiling {
namespace {

AllocatorType ConvertType(base::PoissonAllocationSampler::AllocatorType type) {
  static_assert(static_cast<uint32_t>(
                    base::PoissonAllocationSampler::AllocatorType::kMax) ==
                    static_cast<uint32_t>(AllocatorType::kCount),
                "AllocatorType lengths do not match.");
  switch (type) {
    case base::PoissonAllocationSampler::AllocatorType::kMalloc:
      return AllocatorType::kMalloc;
    case base::PoissonAllocationSampler::AllocatorType::kPartitionAlloc:
      return AllocatorType::kPartitionAlloc;
    case base::PoissonAllocationSampler::AllocatorType::kBlinkGC:
      return AllocatorType::kOilpan;
    default:
      NOTREACHED();
      return AllocatorType::kMalloc;
  }
}

}  // namespace

SamplingProfilerWrapper::SamplingProfilerWrapper() {
  base::PoissonAllocationSampler::Get()->AddSamplesObserver(this);
}

SamplingProfilerWrapper::~SamplingProfilerWrapper() {
  base::PoissonAllocationSampler::Get()->RemoveSamplesObserver(this);
}

void SamplingProfilerWrapper::StartProfiling(size_t sampling_rate) {
  auto* sampler = base::PoissonAllocationSampler::Get();
  sampler->SetSamplingInterval(sampling_rate);
  sampler->Start();
}

void SamplingProfilerWrapper::SampleAdded(
    void* address,
    size_t size,
    size_t total,
    base::PoissonAllocationSampler::AllocatorType type,
    const char* context) {
  RecordAndSendAlloc(ConvertType(type), address, size, context);
}

void SamplingProfilerWrapper::SampleRemoved(void* address) {
  RecordAndSendFree(address);
}

}  // namespace heap_profiling
