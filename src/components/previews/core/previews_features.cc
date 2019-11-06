// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_features.h"

#include "build/build_config.h"

namespace previews {
namespace features {

// Kill switch (or holdback) for all previews. No previews will be allowed
// if this feature is disabled. If enabled, which specific previews that
// are enabled are controlled by other features.
const base::Feature kPreviews {
  "Previews",
#if defined(OS_ANDROID) || defined(OS_LINUX)
      // Previews allowed for Android (but also allow on Linux for dev/debug).
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // !defined(OS_ANDROID) || defined(OS_LINUX)
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_ANDROID) || defined(OS_LINUX)
};

// Enables the Offline previews on android slow connections.
const base::Feature kOfflinePreviews{"OfflinePreviews",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Client Lo-Fi previews.
const base::Feature kClientLoFi{"ClientLoFi",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the NoScript previews for Android.
const base::Feature kNoScriptPreviews {
  "NoScriptPreviews",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // !defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_ANDROID)
};

// Enables the Stale Previews timestamp on Previews infobars.
const base::Feature kStalePreviewsTimestamp{"StalePreviewsTimestamp",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the syncing of the Optimization Hints component, which provides
// hints for what Previews can be applied on a page load.
const base::Feature kOptimizationHints {
  "OptimizationHints",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // !defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_ANDROID)
};

// Enables Optimization Hints that are marked as experimental. Optimizations are
// marked experimental by setting an experiment name in the "experiment_name"
// field of the Optimization proto. This allows experiments at the granularity
// of a single PreviewType for a single host (or host suffix). The intent is
// that optimizations that may not work properly for certain sites can be tried
// at a small scale via Finch experiments. Experimental optimizations can be
// activated by enabling this feature and passing an experiment name as a
// parameter called "experiment_name" that matches the experiment name in the
// Optimization proto.
const base::Feature kOptimizationHintsExperiments{
    "OptimizationHintsExperiments", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the application of the resource loading hints when loading resources.
const base::Feature kResourceLoadingHints {
  "ResourceLoadingHints",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // !defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_ANDROID)
};

// Enables client redirects to a server-rendered lite page preview.
const base::Feature kLitePageServerPreviews{"LitePageServerPreviews",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Provides slow page triggering parameters.
const base::Feature kSlowPageTriggering{"PreviewsSlowPageTriggering",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Allows HTTPS previews to be served via a URLLoader when network service is
// enabled.
const base::Feature kHTTPSServerPreviewsUsingURLLoader{
    "HTTPSServerPreviewsUsingURLLoader", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables fetching optimization hints from a remote Optimization Guide Service.
const base::Feature kOptimizationHintsFetching{
    "OptimizationHintsFetching", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the use of a pref to only trigger Offline Previews when there is a
// high chance that there is one to serve.
const base::Feature kOfflinePreviewsFalsePositivePrevention{
    "OfflinePreviewsFalsePositivePrevention",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a per-page load holdback experiment using a random coin flip.
const base::Feature kCoinFlipHoldback{"PreviewsCoinFlipHoldback_UKMOnly",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables filtering navigation URLs by suffix to exclude navigation that look
// like media resources from triggering previews. For example,
// http://chromium.org/video.mp4 would be excluded.
const base::Feature kExcludedMediaSuffixes{"PreviewsExcludedMediaSuffixes",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables DeferAllScript previews.
const base::Feature kDeferAllScriptPreviews{"DeferAllScript",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace previews
