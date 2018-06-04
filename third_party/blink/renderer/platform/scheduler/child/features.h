// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_CHILD_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_CHILD_FEATURES_H_

#include "base/feature_list.h"

namespace blink {
namespace scheduler {

const base::Feature kHighPriorityInput{"BlinkSchedulerHighPriorityInput",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDedicatedWorkerThrottling{
    "BlinkSchedulerWorkerThrottling", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of background (with no audio) pages'
// task queues to low priority.
const base::Feature kLowPriorityForBackgroundPages{
    "BlinkSchedulerLowPriorityForBackgroundPages",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of background (with no audio) pages'
// task queues to best effort.
const base::Feature kBestEffortPriorityForBackgroundPages{
    "BlinkSchedulerBestEffortPriorityForBackgroundPages",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of sub-frame task queues to low
// priority during the loading use case.
const base::Feature kLowPriorityForSubFrameDuringLoading{
    "BlinkSchedulerLowPriorityForSubFrameDuringLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of throttleable task queues to
// low priority during the loading use case.
const base::Feature kLowPriorityForThrottleableTaskDuringLoading{
    "BlinkSchedulerLowPriorityForThrottleableTaskDuringLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of sub-frame throttleable
// task queues to low priority during the loading use case.
const base::Feature kLowPriorityForSubFrameThrottleableTaskDuringLoading{
    "BlinkSchedulerLowPriorityForSubFrameThrottleableTaskDuringLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of hidden frame task queues to
// low priority during the loading use case.
const base::Feature kLowPriorityForHiddenFrameDuringLoading{
    "BlinkSchedulerLowPriorityForHiddenFrameDuringLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_CHILD_FEATURES_H_
