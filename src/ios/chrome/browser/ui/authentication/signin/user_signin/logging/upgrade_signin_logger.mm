// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/upgrade_signin_logger.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "net/base/network_change_notifier.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::LogSigninAccessPointStarted;
using signin_metrics::PromoAction;
using signin_metrics::RecordSigninUserActionForAccessPoint;

namespace {
// Key in the UserDefaults to track how many times the SSO Recall promo has been
// displayed.
NSString* kDisplayedSSORecallPromoCountKey = @"DisplayedSSORecallPromoCount";
// Name of the UMA SSO Recall histogram.
const char* const kUMASSORecallPromoAction = "SSORecallPromo.PromoAction";
// Name of the histogram recording how many accounts were available on the
// device when the promo was shown.
const char* const kUMASSORecallAccountsAvailable =
    "SSORecallPromo.AccountsAvailable";
// Name of the histogram recording how many times the promo has been shown.
const char* const kUMASSORecallPromoSeenCount = "SSORecallPromo.PromoSeenCount";

// Values of the UMA SSORecallPromo.PromoAction histogram.
typedef NS_ENUM(NSUInteger, UserSigninPromoAction) {
  PromoActionDismissed,
  PromoActionEnabledSSOAccount,
  PromoActionAddedAnotherAccount,
  PromoActionCount
};
}  // namespace

@implementation UpgradeSigninLogger

#pragma mark - Public

- (void)logSigninStarted {
  [super logSigninStarted];
  RecordSigninUserActionForAccessPoint(self.accessPoint, self.promoAction);

  // Records in user defaults that the promo has been shown as well as the
  // number of times it's been displayed.
  SigninRecordVersionSeen();
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  int promoSeenCount =
      [standardDefaults integerForKey:kDisplayedSSORecallPromoCountKey];
  promoSeenCount++;
  [standardDefaults setInteger:promoSeenCount
                        forKey:kDisplayedSSORecallPromoCountKey];

  NSArray* identities = ios::GetChromeBrowserProvider()
                            ->GetChromeIdentityService()
                            ->GetAllIdentitiesSortedForDisplay();
  UMA_HISTOGRAM_COUNTS_100(kUMASSORecallAccountsAvailable, [identities count]);
  UMA_HISTOGRAM_COUNTS_100(kUMASSORecallPromoSeenCount, promoSeenCount);
}

- (void)logSigninCompletedWithResult:(SigninCoordinatorResult)signinResult
                         addedAcount:(BOOL)addedAccount
               advancedSettingsShown:(BOOL)advancedSettingsShown {
  [super logSigninCompletedWithResult:signinResult
                         addedAccount:addedAccount
                advancedSettingsShown:advancedSettingsShown];
  switch (signinResult) {
    case SigninCoordinatorResultSuccess: {
      UserSigninPromoAction promoAction = addedAccount
                                              ? PromoActionAddedAnotherAccount
                                              : PromoActionEnabledSSOAccount;
      UMA_HISTOGRAM_ENUMERATION(kUMASSORecallPromoAction, promoAction,
                                PromoActionCount);
      break;
    }
    case SigninCoordinatorResultCanceledByUser: {
      UMA_HISTOGRAM_ENUMERATION(kUMASSORecallPromoAction, PromoActionDismissed,
                                PromoActionCount);
      break;
    }
    case SigninCoordinatorResultInterrupted: {
      // TODO(crbug.com/951145): Add metric for when the sign-in has been
      // interrupted.
      break;
    }
  }
}

@end
