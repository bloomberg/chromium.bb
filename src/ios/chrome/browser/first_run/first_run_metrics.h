// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_FIRST_RUN_METRICS_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_FIRST_RUN_METRICS_H_

@protocol FirstRunMetricsDelegate
// A callback function to whichever object is keeping track of whether
// a user has attempted to sign in during First Run Sign-in flow.
- (void)setSignInAttempted;
@end

namespace first_run {

// The different First Run Chrome Login outcomes for users.
enum SignInStatus {
  // User skipped sign in by clicking on Skip at the first opportunity.
  SIGNIN_SKIPPED_QUICK,
  // User signed in to Chrome successfully at First Run.
  SIGNIN_SUCCESSFUL,
  // User attempted to sign in, but gave up by clicking on Skip after trying.
  SIGNIN_SKIPPED_GIVEUP,
  // SSO account exists and user skipped sign in by clicking on Skip at the
  // first opportunity.
  HAS_SSO_ACCOUNT_SIGNIN_SKIPPED_QUICK,
  // SSO account exists and user signed in to Chrome successfully at First Run.
  HAS_SSO_ACCOUNT_SIGNIN_SUCCESSFUL,
  // SSO account exists and user attempted to sign in, but gave up by clicking
  // on Skip after trying.
  HAS_SSO_ACCOUNT_SIGNIN_SKIPPED_GIVEUP,
  // Sentinel file marks the successful completion of First Run. This records
  // the cases where sentinel creation failed. In most likelihood, user will
  // go through First Run again at the next launch - deprecated.
  SENTINEL_CREATION_FAILED,
  // Number of First Run states.
  SIGNIN_SIZE
};

// Starting with iOS 6, Mobile Safari supports Smart App Banners which
// can direct users into AppStore to download another app and then launches
// the freshly installed app. This UMA histogram tracks the number of
// Chrome application launched for the first time (First Run) as the
// result of -openURL: call by another application. Note that there is no
// 100%-sure way of telling if a launch is due to Smart App Banners.
enum ExternalLaunch {
  // Chrome was launched for the first time from Mobile Safari and there is
  // sufficient evidence (e.g. via URL parameters) that it was the result of
  // a Smart App Banner.
  LAUNCH_BY_SMARTAPPBANNER,
  // Chrome was launched for the first time from Mobile Safari, but there is
  // not sufficient indicator to show that it was launched as a result of a
  // click on Smart App Banner.
  LAUNCH_BY_MOBILESAFARI,
  // Chrome was launch for the first time by some other applications.
  LAUNCH_BY_OTHERS,
  // Number of ways that Chrome was launched for the first time.
  LAUNCH_SIZE
};

}  // namespace first_run

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_FIRST_RUN_METRICS_H_
