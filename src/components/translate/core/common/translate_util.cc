// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_util.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/translate/core/common/translate_switches.h"

namespace translate {

namespace {

// Parameter for TranslateSubFrames feature to determine whether language
// detection should include the sub frames (or just the main frame).
const char kDetectLanguageInSubFrames[] = "detect_language_in_sub_frames";

}  // namespace

const char kSecurityOrigin[] = "https://translate.googleapis.com/";

const base::Feature kTranslateSubFrames{"TranslateSubFrames",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTFLiteLanguageDetectionEnabled{
  "TFLiteLanguageDetectionEnabled",
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_WIN) || \
    defined(OS_MAC)
      base::FEATURE_ENABLED_BY_DEFAULT
#else  // !defined(OS_LINUX) && !defined(OS_CHROMEOS) && !defined(OS_WIN) &&
       // !defined(OS_MAC)
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

GURL GetTranslateSecurityOrigin() {
  std::string security_origin(kSecurityOrigin);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTranslateSecurityOrigin)) {
    security_origin =
        command_line->GetSwitchValueASCII(switches::kTranslateSecurityOrigin);
  }
  return GURL(security_origin);
}

bool IsSubFrameTranslationEnabled() {
  return base::FeatureList::IsEnabled(kTranslateSubFrames);
}

bool IsSubFrameLanguageDetectionEnabled() {
  return base::FeatureList::IsEnabled(kTranslateSubFrames) &&
         base::GetFieldTrialParamByFeatureAsBool(
             kTranslateSubFrames, kDetectLanguageInSubFrames, true);
}

bool IsTFLiteLanguageDetectionEnabled() {
  return base::FeatureList::IsEnabled(kTFLiteLanguageDetectionEnabled);
}

float GetTFLiteLanguageDetectionThreshold() {
  return base::GetFieldTrialParamByFeatureAsDouble(
      kTFLiteLanguageDetectionEnabled, "reliability_threshold", .7);
}

}  // namespace translate
