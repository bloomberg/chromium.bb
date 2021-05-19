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

// Enables client side detection on Android.
extern const base::Feature kClientSideDetectionForAndroid;

// Determines the experimental version of client side detection model, for
// Desktop.
extern const base::Feature kClientSideDetectionModelVersion;

// Enables client side detection referrer chain.
extern const base::Feature kClientSideDetectionReferrerChain;

// Enables GAIA-keying of client side detection requests for Enhanced Safe
// Browsing users.
extern const base::Feature kClientSideDetectionWithToken;

// Enable the addition of access tokens to download pings for enhanced
// protection users.
extern const base::Feature kDownloadRequestWithToken;

// Controls whether the limited list size experiment is enabled. This experiment
// limits the number of entries stored in each Safe Browsing list.
extern const base::Feature kLimitedListSizeForIOS;

// Include referring app info in password protection requests on Android.
extern const base::Feature kPasswordProtectionReferringAppEnabledAndroid;

// Enable GAIA password protection for signed-in users.
extern const base::Feature kPasswordProtectionForSignedInUsers;

// Enables GAIA-keying of password protection requests for Enhanced Safe
// Browsing users.
extern const base::Feature kPasswordProtectionWithToken;

// Controls whether Chrome prompts Enhanced Safe Browsing users for deep
// scanning.
extern const base::Feature kPromptEsbForDeepScanning;

// Controls whether we are performing enterprise download checks for users with
// the appropriate policies enabled.
extern const base::Feature kSafeBrowsingEnterpriseCsd;

// Controls whether we are disabling consumer download checks for users using
// the enterprise download checks.
extern const base::Feature kSafeBrowsingDisableConsumerCsdForEnterprise;

// Controls whether Safe Browsing uses separate NetworkContexts for each
// profile.
extern const base::Feature kSafeBrowsingSeparateNetworkContexts;

// Controls whether cookies are removed from certain communications with Safe
// Browsing.
extern const base::Feature kSafeBrowsingRemoveCookies;

// Controls the daily quota for the suspicious site trigger.
extern const base::Feature kSuspiciousSiteTriggerQuotaFeature;

// Controls whether the real time URL lookup is enabled.
extern const base::Feature kRealTimeUrlLookupEnabled;

// Controls whether to do real time URL lookup for enterprise users. If both
// this feature and the enterprise policies are enabled, the enterprise real
// time URL lookup will be enabled and the consumer real time URL lookup will be
// disabled.
extern const base::Feature kRealTimeUrlLookupEnabledForEnterprise;

// Controls whether to use the GA endpoint for enterprise real time URL lookup.
extern const base::Feature kRealTimeUrlLookupEnterpriseGaEndpoint;

// Controls whether the GAIA-keyed real time URL lookup is enabled.
extern const base::Feature kRealTimeUrlLookupEnabledWithToken;

// Controls whether the referrer chain is attached to real time requests.
extern const base::Feature kRealTimeUrlLookupReferrerChain;

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

// Controls whether we include visual features in password protection pings on
// Android.
extern const base::Feature kVisualFeaturesInPasswordProtectionAndroid;

// Controls whether the delayed warning experiment is enabled.
extern const base::Feature kDelayedWarnings;
// True if mouse clicks should undelay the warnings immediately when delayed
// warnings feature is enabled.
extern const base::FeatureParam<bool> kDelayedWarningsEnableMouseClicks;

// Status of the SimplifiedUrlDisplay experiments. This does not control the
// individual experiments, those are controlled by their own feature flags.
// The feature is only set by Finch so that we can differentiate between
// default and control groups of the experiment.
extern const base::Feature kSimplifiedUrlDisplay;

base::ListValue GetFeatureStatusList();

// Returns whether or not to stop filling in the SyncAccountType and
// ReusedPasswordType enums. This is used in the
// |kPasswordProtectionForSignedInUsers| experiment.
bool GetShouldFillOldPhishGuardProto();

}  // namespace safe_browsing
#endif  // COMPONENTS_SAFE_BROWSING_CORE_FEATURES_H_
