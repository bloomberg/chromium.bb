// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/common/content_capture_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace content_capture {
namespace features {

#if defined(OS_ANDROID)
const base::Feature kContentCapture{"ContentCapture",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kContentCaptureTriggeringForExperiment{
    "ContentCaptureTriggeringForExperiment", base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kContentCapture{"ContentCapture",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContentCaptureTriggeringForExperiment{
    "ContentCaptureTriggeringForExperiment", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kContentCaptureInWebLayer{
    "ContentCaptureInWebLayer", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsContentCaptureEnabled() {
  return base::FeatureList::IsEnabled(kContentCapture);
}

bool ShouldTriggerContentCaptureForExperiment() {
  return base::FeatureList::IsEnabled(kContentCaptureTriggeringForExperiment);
}

bool IsContentCaptureEnabledInWebLayer() {
  return base::FeatureList::IsEnabled(kContentCaptureInWebLayer);
}

int TaskInitialDelayInMilliseconds() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kContentCapture, "task_initial_delay_in_milliseconds", 500);
}

}  // namespace features
}  // namespace content_capture
