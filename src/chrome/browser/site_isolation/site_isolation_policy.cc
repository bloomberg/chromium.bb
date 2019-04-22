// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_isolation/site_isolation_policy.h"

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/site_isolation_policy.h"

// static
bool SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled() {
  // Ignore attempts to add new isolated origins when site isolation is turned
  // off, for example via a command-line switch, or via a content/ embedder
  // that turns site isolation off for low-memory devices.
  if (!content::SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled())
    return false;

  // The feature needs to be checked last, because checking the feature
  // activates the field trial and assigns the client either to a control or an
  // experiment group - such assignment should be final.
  return base::FeatureList::IsEnabled(features::kSiteIsolationForPasswordSites);
}

// static
bool SiteIsolationPolicy::IsEnterprisePolicyApplicable() {
#if defined(OS_ANDROID)
  // https://crbug.com/844118: Limiting policy to devices with > 1GB RAM.
  // Using 1077 rather than 1024 because 1) it helps ensure that devices with
  // exactly 1GB of RAM won't get included because of inaccuracies or off-by-one
  // errors and 2) this is the bucket boundary in Memory.Stats.Win.TotalPhys2.
  bool have_enough_memory = base::SysInfo::AmountOfPhysicalMemoryMB() > 1077;
  return have_enough_memory;
#else
  return true;
#endif
}
