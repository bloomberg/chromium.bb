// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_features.h"

#include "base/feature_list.h"

namespace base {

const Feature kAllTasksUserBlocking{"AllTasksUserBlocking",
                                    FEATURE_DISABLED_BY_DEFAULT};

const Feature kNoDetachBelowInitialCapacity = {
    "NoDetachBelowInitialCapacity", base::FEATURE_DISABLED_BY_DEFAULT};

const Feature kMayBlockWithoutDelay = {"MayBlockWithoutDelay",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const Feature kDisableJobYield = {"DisableJobYield",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const Feature kDisableFairJobScheduling = {"DisableFairJobScheduling",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const Feature kDisableJobUpdatePriority = {"DisableJobUpdatePriority",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const Feature kWakeUpStrategyFeature = {"WakeUpStrategyFeature",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

constexpr FeatureParam<WakeUpStrategy>::Option kWakeUpStrategyOptions[] = {
    {WakeUpStrategy::kCentralizedWakeUps, "centralized-wakeups"},
    {WakeUpStrategy::kSerializedWakeUps, "serialized-wakeups"},
    {WakeUpStrategy::kExponentialWakeUps, "exponential-wakeups"}};

const base::FeatureParam<WakeUpStrategy> kWakeUpStrategyParam{
    &kWakeUpStrategyFeature, "strategy", WakeUpStrategy::kExponentialWakeUps,
    &kWakeUpStrategyOptions};

const Feature kWakeUpAfterGetWork = {"WakeUpAfterGetWork",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_WIN) || defined(OS_APPLE)
const Feature kUseNativeThreadPool = {"UseNativeThreadPool",
                                      base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const Feature kUseFiveMinutesThreadReclaimTime = {
    "UseFiveMinutesThreadReclaimTime", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace base
