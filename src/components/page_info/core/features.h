// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_FEATURES_H_
#define COMPONENTS_PAGE_INFO_CORE_FEATURES_H_

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace base {
struct Feature;
}  // namespace base

namespace page_info {

#if BUILDFLAG(IS_ANDROID)
// Enables the history sub page for Page Info.
extern const base::Feature kPageInfoHistory;
// Enables the store info row for Page Info.
extern const base::Feature kPageInfoStoreInfo;

// Used to experiment with different permission timeouts. The underlying feature
// itself is already launched.
extern const base::Feature kPageInfoDiscoverability;
#endif

// Enables the "About this site" section in Page Info.
extern const base::Feature kPageInfoAboutThisSite;

// Whether we show hard-coded content for some sites like https://example.com.
extern const base::FeatureParam<bool> kShowSampleContent;

#if !BUILDFLAG(IS_ANDROID)
// Enables the history section for Page Info on desktop.
extern const base::Feature kPageInfoHistoryDesktop;
#endif

}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_FEATURES_H_
