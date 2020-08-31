// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_FEATURES_H_
#define COMPONENTS_SAFE_BROWSING_CORE_FEATURES_H_

#include <stddef.h>
#include <algorithm>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/values.h"
namespace base {
class ListValue;
}  // namespace base

namespace safe_browsing {
// Features list, in alphabetical order.

// Controls whether we send RIND reports when a popup originating from a Google
// Ad is blocked.
extern const base::Feature kAdPopupTriggerFeature;

// Controls whether we send RIND reports when a redirect caused by a Google Ad
// is blocked.
extern const base::Feature kAdRedirectTriggerFeature;

extern const base::Feature kAdSamplerTriggerFeature;

// Controls whether we sample inline JavaScript for ads in RIND
// reports.
extern const base::Feature kCaptureInlineJavascriptForGoogleAds;

// Controls whether to do deep scanning for DLP. If both this feature and
// the enterprise policies are enabled, the downloaded and uploaded files are
// sent for scanning.
extern const base::Feature kContentComplianceEnabled;

// Enable the addition of access tokens to download pings for enhanced
// protection users.
extern const base::Feature kDownloadRequestWithToken;

// Enable Chrome Safe Browsing enhanced protection.
extern const base::Feature kEnhancedProtection;

// Controls whether to do deep scanning for malware. If both this feature and
// the enterprise policies are enabled, the downloaded and uploaded files are
// sent for scanning.
extern const base::Feature kMalwareScanEnabled;

// Enable password protection for non-Google accounts.
extern const base::Feature kPasswordProtectionForSavedPasswords;

// Enable whether or not to show a list of domains the saved password was used
// on the modal warning dialog during password protection. This is only checked
// if the |kPasswordProtectionForSavedPasswords| experiment is on.
extern const base::Feature kPasswordProtectionShowDomainsForSavedPasswords;

// Enable GAIA password protection for signed-in users.
extern const base::Feature kPasswordProtectionForSignedInUsers;

// Controls whether Chrome prompts Advanced Protection users for deep scanning.
extern const base::Feature kPromptAppForDeepScanning;

// Controls whether native (instead of WKWebView-provided) Safe Browsing
// is available on iOS. When this flag is enabled, Safe Browsing is still
// subject to an opt-out controlled by prefs::kSafeBrowsingEnabled.
extern const base::Feature kSafeBrowsingAvailableOnIOS;

// Controls whether Safe Browsing uses separate NetworkContexts for each
// profile.
extern const base::Feature kSafeBrowsingSeparateNetworkContexts;

// Controls the daily quota for the suspicious site trigger.
extern const base::Feature kSuspiciousSiteTriggerQuotaFeature;

// Controls whether the real time URL lookup is enabled.
extern const base::Feature kRealTimeUrlLookupEnabled;

// Controls whether the real time URL lookup is enabled for all Android devices.
// This flag is in effect only if |kRealTimeUrlLookupEnabled| is true.
extern const base::Feature kRealTimeUrlLookupEnabledForAllAndroidDevices;

// Controls whether the real time URL lookup is enabled for Enhanced Protection
// users.
extern const base::Feature kRealTimeUrlLookupEnabledForEP;

// Controls whether the GAIA-keyed real time URL lookup is enabled for Enhanced
// Protection users.
extern const base::Feature kRealTimeUrlLookupEnabledForEPWithToken;

// Controls whether the GAIA-keyed real time URL lookup is enabled.
extern const base::Feature kRealTimeUrlLookupEnabledWithToken;

// Controls whether the real time URL lookup is enabled for non mainframe URLs
// for Enhanced Protection users.
extern const base::Feature kRealTimeUrlLookupNonMainframeEnabledForEP;

// Specifies which non-resource HTML Elements to collect based on their tag and
// attributes. It's a single param containing a comma-separated list of pairs.
// For example: "tag1,id,tag1,height,tag2,foo" - this will collect elements with
// tag "tag1" that have attribute "id" or "height" set, and elements of tag
// "tag2" if they have attribute "foo" set. All tag names and attributes should
// be lower case.
extern const base::Feature kThreatDomDetailsTagAndAttributeFeature;

// Controls the daily quota for data collection triggers. It's a single param
// containing a comma-separated list of pairs. The format of the param is
// "T1,Q1,T2,Q2,...Tn,Qn", where Tx is a TriggerType and Qx is how many reports
// that trigger is allowed to send per day.
// TODO(crbug.com/744869): This param should be deprecated after ad sampler
// launch in favour of having a unique quota feature and param per trigger.
// Having a single shared feature makes it impossible to run multiple trigger
// trials simultaneously.
extern const base::Feature kTriggerThrottlerDailyQuotaFeature;

// Controls whether Chrome uses new download warning UX.
extern const base::Feature kUseNewDownloadWarnings;

// Controls whether the delayed warning experiment is enabled.
extern const base::Feature kDelayedWarnings;

base::ListValue GetFeatureStatusList();

// Returns whether or not to stop filling in the SyncAccountType and
// ReusedPasswordType enums. This is used in the
// |kPasswordProtectionForSignedInUsers| experiment.
bool GetShouldFillOldPhishGuardProto();

}  // namespace safe_browsing
#endif  // COMPONENTS_SAFE_BROWSING_CORE_FEATURES_H_
