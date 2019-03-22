// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/gwp_asan.h"

#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/client/sampling_allocator_shims.h"

namespace gwp_asan {

namespace internal {
namespace {

const base::Feature kGwpAsan{"GwpAsanMalloc",
                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kAllocationsParam{&kGwpAsan, "TotalAllocations",
                                                AllocatorState::kGpaMaxPages};

const base::FeatureParam<int> kAllocationSamplingParam{
    &kGwpAsan, "AllocationSamplingFrequency", 128};

const base::FeatureParam<double> kProcessSamplingParam{
    &kGwpAsan, "ProcessSamplingProbability", 1.0};

bool EnableForMalloc() {
  if (!base::FeatureList::IsEnabled(kGwpAsan))
    return false;

  int total_allocations = kAllocationsParam.Get();
  if (total_allocations < 1 ||
      total_allocations >
          base::checked_cast<int>(AllocatorState::kGpaMaxPages)) {
    DLOG(ERROR) << "GWP-ASan TotalAllocations is out-of-range: "
                << total_allocations;
    return false;
  }

  int alloc_sampling_freq = kAllocationSamplingParam.Get();
  if (alloc_sampling_freq < 1) {
    DLOG(ERROR) << "GWP-ASan AllocationSamplingFrequency is out-of-range: "
                << alloc_sampling_freq;
    return false;
  }

  double process_sampling_probability = kProcessSamplingParam.Get();
  if (process_sampling_probability < 0.0 ||
      process_sampling_probability > 1.0) {
    DLOG(ERROR) << "GWP-ASan ProcessSamplingProbability is out-of-range: "
                << process_sampling_probability;
    return false;
  }

  if (base::RandDouble() >= process_sampling_probability)
    return false;

  InstallAllocatorHooks(total_allocations, alloc_sampling_freq);
  return true;
}

}  // namespace
}  // namespace internal

void EnableForMalloc() {
  static bool init_once = internal::EnableForMalloc();
  ignore_result(init_once);
}

}  // namespace gwp_asan
