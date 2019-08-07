// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The command line values for the content inset and safe area experiment
// choices.
const char kFrameChoiceValue[] = "frame";
const char kContentInsetChoiceValue[] = "content-inset";
const char kSafeAreaChoiceValue[] = "safe-area";
const char kHybridChoiceValue[] = "hybrid";
const char kSmoothScrollingChoiceValue[] = "smooth";

// Feature used by finch config to enable smooth scrolling when the default
// viewport adjustment experiment is selected via command line switches.
const base::Feature kSmoothScrollingDefault{"FullscreenSmoothScrollingDefault",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
}

namespace fullscreen {
namespace features {

const char kViewportAdjustmentExperimentCommandLineSwitch[] =
    "fullscreen-viewport-adjustment-experiment";

const flags_ui::FeatureEntry::Choice kViewportAdjustmentExperimentChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"Update Content Inset", kViewportAdjustmentExperimentCommandLineSwitch,
     "content-inset"},
    {"Update Safe Area", kViewportAdjustmentExperimentCommandLineSwitch,
     "safe-area"},
    {"Use Hybrid Implementation",
     kViewportAdjustmentExperimentCommandLineSwitch, "hybrid"},
    {"Use Smooth Scrolling", kViewportAdjustmentExperimentCommandLineSwitch,
     "smooth"},
    {"Update Frame", kViewportAdjustmentExperimentCommandLineSwitch, "frame"}};

ViewportAdjustmentExperiment GetActiveViewportExperiment() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kViewportAdjustmentExperimentCommandLineSwitch)) {
    std::string viewport_experiment = command_line->GetSwitchValueASCII(
        kViewportAdjustmentExperimentCommandLineSwitch);
    if (viewport_experiment == std::string(kContentInsetChoiceValue))
      return ViewportAdjustmentExperiment::CONTENT_INSET;
    if (viewport_experiment == std::string(kSafeAreaChoiceValue))
      return ViewportAdjustmentExperiment::SAFE_AREA;
    if (viewport_experiment == std::string(kHybridChoiceValue))
      return ViewportAdjustmentExperiment::HYBRID;
    if (viewport_experiment == std::string(kSmoothScrollingChoiceValue))
      return ViewportAdjustmentExperiment::SMOOTH_SCROLLING;
    if (viewport_experiment == std::string(kFrameChoiceValue))
      return ViewportAdjustmentExperiment::FRAME;
  }
  return base::FeatureList::IsEnabled(kSmoothScrollingDefault)
             ? ViewportAdjustmentExperiment::SMOOTH_SCROLLING
             : ViewportAdjustmentExperiment::FRAME;
}

const base::Feature kLockBottomToolbar{"LockBottomToolbar",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace fullscreen
