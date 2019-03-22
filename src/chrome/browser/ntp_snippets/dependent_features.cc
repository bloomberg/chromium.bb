// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_snippets/dependent_features.h"

#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "components/ntp_snippets/features.h"
#include "components/offline_pages/core/offline_page_feature.h"

namespace ntp_snippets {

// All platforms proxy for whether the simplified NTP is enabled.
bool IsSimplifiedNtpEnabled() {
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif  // OS_ANDROID
}

bool AreAssetDownloadsEnabled() {
  return !IsSimplifiedNtpEnabled() &&
         base::FeatureList::IsEnabled(
             features::kAssetDownloadSuggestionsFeature);
}

bool AreOfflinePageDownloadsEnabled() {
  return !IsSimplifiedNtpEnabled() &&
         base::FeatureList::IsEnabled(
             features::kOfflinePageDownloadSuggestionsFeature);
}
bool IsDownloadsProviderEnabled() {
  return AreAssetDownloadsEnabled() || AreOfflinePageDownloadsEnabled();
}

bool IsBookmarkProviderEnabled() {
  return !IsSimplifiedNtpEnabled() &&
         base::FeatureList::IsEnabled(
             ntp_snippets::kBookmarkSuggestionsFeature);
}

}  // namespace ntp_snippets
