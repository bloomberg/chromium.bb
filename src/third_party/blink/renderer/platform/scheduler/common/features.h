// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {
namespace scheduler {

const base::Feature kHighPriorityInputOnCompositorThread{
    "BlinkSchedulerHighPriorityInputOnCompositorThread",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDedicatedWorkerThrottling{
    "BlinkSchedulerWorkerThrottling", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBestEffortPriorityForFindInPage{
    "BlinkSchedulerBestEffortPriorityForFindInPage",
    base::FEATURE_DISABLED_BY_DEFAULT};

// COMPOSITING PRIORITY EXPERIMENT CONTROLS

// If enabled, the compositor will always be set to kVeryHighPriority if it
// is not already set to kHighestPriority.
const base::Feature kVeryHighPriorityForCompositingAlways{
    "BlinkSchedulerVeryHighPriorityForCompositingAlways",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, compositor priority will be set to kVeryHighPriority if it will
// be fast and is not already set to kHighestPriority.
const base::Feature kVeryHighPriorityForCompositingWhenFast{
    "BlinkSchedulerVeryHighPriorityForCompositingWhenFast",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, compositor priority will be set to kVeryHighPriority if the last
// task completed was not a compositor task, and kNormalPriority if the last
// task completed was a compositor task.
const base::Feature kVeryHighPriorityForCompositingAlternating{
    "BlinkSchedulerVeryHighPriorityForCompositingAlternating",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, compositor priority will be set to kVeryHighPriority if no
// compositor task has run for some time determined by the finch parameter
// kCompositingDelayLength. Once a compositor task runs, it will be reset
// to kNormalPriority.
const base::Feature kVeryHighPriorityForCompositingAfterDelay{
    "BlinkSchedulerVeryHighPriorityForCompositingAfterDelay",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Param for kVeryHighPriorityForCompositingAfterDelay experiment. How long
// in ms the compositor will wait to be prioritized if no compositor tasks run.
constexpr base::FeatureParam<int> kCompositingDelayLength{
    &kVeryHighPriorityForCompositingAfterDelay, "CompositingDelayLength", 100};

// If enabled, compositor priority will be set to kVeryHighPriority until
// a budget has been exhausted. Once the budget runs out, the priority will
// be set to kNormalPriority until there is enough budget to reprioritize.
const base::Feature kVeryHighPriorityForCompositingBudget{
    "BlinkSchedulerVeryHighPriorityForCompositingBudget",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Param for kVeryHighPriorityForCompositingBudget experiment. This param
// controls how much CPU time the compositor will be prioritized for, its
// budget. Measured in ms.
constexpr base::FeatureParam<int> kInitialCompositorBudgetInMilliseconds{
    &kVeryHighPriorityForCompositingBudget,
    "InitialCompositorBudgetInMilliseconds", 250};

// Param for kVeryHighPriorityForCompositingBudget experiment. This param
// controls the rate at which the budget is recovered.
constexpr base::FeatureParam<double> kCompositorBudgetRecoveryRate{
    &kVeryHighPriorityForCompositingBudget, "CompositorBudgetRecoveryRate",
    0.25};

// This feature functions as an experiment parameter for the
// VeryHighPriorityForCompositing alternating, delay, and budget experiments.
// When enabled, it does nothing unless one of these experiments is also
// enabled. If one of these experiments is enabled it will change the behavior
// of that experiment such that the stop signal for prioritzation of the
// compositor is a BeginMainFrame task instead of any compositor task.
const base::Feature kPrioritizeCompositingUntilBeginMainFrame{
    "BlinkSchedulerPrioritizeCompositingUntilBeginMainFrame",
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

// Used along with |kLowPriorityForHiddenFrame|,
// |kLowPriorityForSubFrameThrottleableTask|, |kLowPriorityForThrottleableTask|,
// |kLowPriorityForSubFrame| to enable one of these experiments only during the
// load use case.
const base::Feature kFrameExperimentOnlyWhenLoading{
    "BlinkSchedulerFrameExperimentOnlyWhenLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of an ad frame to low priority.
const base::Feature kLowPriorityForAdFrame{
    "BlinkSchedulerLowPriorityForAdFrame", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of an ad frame to best effort priority.
const base::Feature kBestEffortPriorityForAdFrame{
    "BlinkSchedulerBestEffortPriorityForAdFrame",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Used along with |kLowPriorityForAdFrame| or |kBestEffortPriorityForAdFrame|
// to enable one of these experiments only during the load use case.
const base::Feature kAdFrameExperimentOnlyWhenLoading{
    "BlinkSchedulerAdFrameExperimentOnlyWhenLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables using a resource's fetch priority to determine the priority of the
// resource's loading tasks posted to blink's scheduler.
const base::Feature kUseResourceFetchPriority{
    "BlinkSchedulerResourceFetchPriority", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables using a resource's fetch priority to determine the priority of the
// resource's loading tasks posted to blink's scheduler only for resources
// requested during the loading phase.
const base::Feature kUseResourceFetchPriorityOnlyWhenLoading{
    "BlinkSchedulerResourceFetchPriorityOnlyWhenLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of cross-origin task queues to
// low priority.
const base::Feature kLowPriorityForCrossOrigin{
    "BlinkSchedulerLowPriorityForCrossOrigin",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of cross-origin task queues to
// low priority during loading only.
const base::Feature kLowPriorityForCrossOriginOnlyWhenLoading{
    "BlinkSchedulerLowPriorityForCrossOriginOnlyWhenLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Prioritizes loading and compositing tasks while loading.
const base::Feature kPrioritizeCompositingAndLoadingDuringEarlyLoading{
    "PrioritizeCompositingAndLoadingDuringEarlyLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable setting high priority database task type from field trial parameters.
const base::Feature kHighPriorityDatabaseTaskType{
    "HighPriorityDatabaseTaskType", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_FEATURES_H_
