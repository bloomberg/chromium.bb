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

// COMPOSITING PRIORITY EXPERIMENT CONTROLS

// Enables experiment to increase priority of the compositing tasks during
// input handling. Other features in this section do not have any effect
// when this feature is disabled.
const base::Feature kPrioritizeCompositingAfterInput{
    "BlinkSchedulerPrioritizeCompositingAfterInput",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Use kHighestPriority for compositing tasks during the experiment.
// kHighPriority is used otherwise.
const base::Feature kHighestPriorityForCompositingAfterInput{
    "BlinkSchedulerHighestPriorityForCompostingAfterInput",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, MainFrameSchedulerImpl::OnRequestMainFrameForInput is used as
// triggering signal for the experiment. If disabled, the presence of an input
// task is used as trigger.
const base::Feature kUseExplicitSignalForTriggeringCompositingPrioritization{
    "BlinkSchedulerUseExplicitSignalForTriggeringCompositingPrioritization",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the increased priority continues until we get the appropriate
// number of WillBeginMainFrame signals. If disabled, the priority is increased
// for the fixed number of compositing tasks.
const base::Feature kUseWillBeginMainFrameForCompositingPrioritization{
    "BlinkSchedulerUseWillBeginMainFrameForCompositingPrioritization",
    base::FEATURE_DISABLED_BY_DEFAULT};

// LOAD PRIORITY EXPERIMENT CONTROLS

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
// priority.
const base::Feature kLowPriorityForSubFrame{
    "BlinkSchedulerLowPriorityForSubFrame", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of throttleable task queues to
// low priority.
const base::Feature kLowPriorityForThrottleableTask{
    "BlinkSchedulerLowPriorityForThrottleableTask",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of sub-frame throttleable
// task queues to low priority.
const base::Feature kLowPriorityForSubFrameThrottleableTask{
    "BlinkSchedulerLowPriorityForSubFrameThrottleableTask",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of hidden frame task queues to
// low priority.
const base::Feature kLowPriorityForHiddenFrame{
    "BlinkSchedulerLowPriorityForHiddenFrame",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a chosen experiments only during the load use case.
const base::Feature kExperimentOnlyWhenLoading{
    "BlinkSchedulerExperimentOnlyWhenLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_CHILD_FEATURES_H_
