// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_POLICY_FEATURES_H_
#define IOS_CHROME_BROWSER_POLICY_POLICY_FEATURES_H_

#include "base/feature_list.h"

// Feature flag for supporting the EditBookmarks enterprise policy on iOS.
extern const base::Feature kEditBookmarksIOS;
// Feature flag for supporting the ManagedBookmarks enterprise policy on iOS.
extern const base::Feature kManagedBookmarksIOS;

// Feature flag for supporting the URLBlocklist enterprise policy on iOS.
extern const base::Feature kURLBlocklistIOS;

// Returns true if EditBookmarksEnabled enterprise policy is supported on iOS.
bool IsEditBookmarksIOSEnabled();

// Returns true if the core enterprise policy infrastructure is enabled. Does
// not control whether policy data is parsed and made user visible; that is
// controlled by |ShouldInstallEnterprisePolicyHandlers()| below.
bool IsEnterprisePolicyEnabled();

// Returns true if enterprise policy handlers should be installed to parse
// policy data and make it user visible.
bool ShouldInstallEnterprisePolicyHandlers();

// Returns true if the ManagedBookmarks policy handler should be installed to
// parse policy data and make it user visible.
bool ShouldInstallManagedBookmarksPolicyHandler();

// Returns true if ManagedBookmarks enterprise policy is enabled.
bool IsManagedBookmarksEnabled();

// Returns true if the URLBlocklist and URLAllowlist policy handlers should be
// installed.
bool ShouldInstallURLBlocklistPolicyHandlers();

// Returns true if URLBlocklist/URLAllowlist enterprise policies are enabled.
bool IsURLBlocklistEnabled();

#endif  // IOS_CHROME_BROWSER_POLICY_POLICY_FEATURES_H_
