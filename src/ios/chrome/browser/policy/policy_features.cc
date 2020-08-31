// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/policy_features.h"

#include "base/command_line.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/common/features.h"

const base::Feature kEditBookmarksIOS{"EditBookmarksIOS",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kManagedBookmarksIOS{"ManagedBookmarksIOS",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kURLBlocklistIOS{"URLBlocklistIOS",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

namespace {

// Returns true if the current command line contains the
// |kDisableEnterprisePolicy| switch.
bool IsDisableEnterprisePolicySwitchPresent() {
  // This feature is controlled via the command line because policy must be
  // initialized before about:flags or field trials. Using a command line flag
  // is the only way to control this feature at runtime.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kDisableEnterprisePolicy);
}

// Returns true if the current command line contains the
// |kEnableEnterprisePolicy| switch.
bool IsEnableEnterprisePolicySwitchPresent() {
  // This feature is controlled via the command line because policy must be
  // initialized before about:flags or field trials. Using a command line flag
  // is the only way to control this feature at runtime.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableEnterprisePolicy);
}

}  // namespace

bool IsEditBookmarksIOSEnabled() {
  return base::FeatureList::IsEnabled(kEditBookmarksIOS);
}

bool IsEnterprisePolicyEnabled() {
  if (IsDisableEnterprisePolicySwitchPresent()) {
    return false;
  }

  // Policy is enabled by default for non-stable channels.
  return GetChannel() != version_info::Channel::STABLE ||
         IsEnableEnterprisePolicySwitchPresent();
}

bool ShouldInstallEnterprisePolicyHandlers() {
  return IsEnterprisePolicyEnabled();
}

bool ShouldInstallManagedBookmarksPolicyHandler() {
  // This feature is controlled via the command line because policy must be
  // initialized before about:flags or field trials. Using a command line flag
  // is the only way to control this feature at runtime.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kInstallManagedBookmarksHandler);
}

bool IsManagedBookmarksEnabled() {
  return ShouldInstallManagedBookmarksPolicyHandler() &&
         base::FeatureList::IsEnabled(kManagedBookmarksIOS);
}

bool ShouldInstallURLBlocklistPolicyHandlers() {
  // This feature is controlled via the command line because policy must be
  // initialized before about:flags or field trials. Using a command line flag
  // is the only way to control this feature at runtime.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kInstallURLBlocklistHandlers);
}

bool IsURLBlocklistEnabled() {
  return ShouldInstallURLBlocklistPolicyHandlers() &&
         base::FeatureList::IsEnabled(kURLBlocklistIOS) &&
         // The error page shown for blocked URLs requires
         // |web::features::kUseJSForErrorPage| to be enabled.
         base::FeatureList::IsEnabled(web::features::kUseJSForErrorPage);
}
