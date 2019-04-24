// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_isolation_policy.h"

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
