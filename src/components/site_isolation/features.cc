// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_isolation/features.h"

#include "build/build_config.h"

namespace site_isolation {
namespace features {

// Controls a mode for dynamically process-isolating sites where the user has
// entered a password.  This is intended to be used primarily when full site
// isolation is turned off.  To check whether this mode is enabled, use
// SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled() rather than
// checking the feature directly, since that decision is influenced by other
// factors as well.
const base::Feature kSiteIsolationForPasswordSites {
  "site-isolation-for-password-sites",
// Enabled by default on Android; see https://crbug.com/849815.  Note that this
// should not affect Android Webview, which does not include this code.
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// Controls a mode for dynamically process-isolating sites where the user has
// logged in via OAuth.  These sites are determined by runtime heuristics.
//
// This is intended to be used primarily when full site isolation is turned
// off.  To check whether this mode is enabled, use
// SiteIsolationPolicy::IsIsolationForOAuthSitesEnabled() rather than
// checking the feature directly, since that decision is influenced by other
// factors as well.
//
// This feature does not affect Android Webview, which does not include this
// code.
const base::Feature kSiteIsolationForOAuthSites{
  "SiteIsolationForOAuthSites",
// Enabled by default on Android only; see https://crbug.com/1206770.
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

// kSiteIsolationMemoryThresholds is checked before individual site isolation
// mode base::Features (such as kSitePerProcess or
// kSiteIsolationForPasswordSites), and (if enabled) can restrict those modes
// to not apply to low-memory devices below a certain memory threshold.  The
// threshold for what is considered a "low memory" device can be set (in MB)
// via field trial params with the names defined below, with independent params
// for strict site isolation (kSitePerProcess) and partial site isolation modes
// (kSiteIsolationForPasswordSites, kSiteIsolationForOAuthSites, etc). These
// thresholds are compared against base::SysInfo::AmountOfPhysicalMemoryMB().
// On devices below the memory threshold, the site isolation features such as
// kSitePerProcess won't be checked at all, and field trials won't activate
// either the control or the experiment group.
const base::Feature kSiteIsolationMemoryThresholds{
    "SiteIsolationMemoryThresholds", base::FEATURE_DISABLED_BY_DEFAULT};
const char kStrictSiteIsolationMemoryThresholdParamName[] =
    "strict_site_isolation_threshold_mb";
const char kPartialSiteIsolationMemoryThresholdParamName[] =
    "partial_site_isolation_threshold_mb";

}  // namespace features
}  // namespace site_isolation
