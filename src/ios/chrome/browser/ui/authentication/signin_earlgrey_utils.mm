// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"

#import <EarlGrey/EarlGrey.h>

#include "base/strings/sys_string_conversions.h"
#include "components/signin/core/browser/account_info.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/nserror_util.h"
#include "services/identity/public/cpp/identity_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SigninEarlGreyUtils

+ (ChromeIdentity*)fakeIdentity1 {
  return [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                        gaiaID:@"foo1ID"
                                          name:@"Fake Foo 1"];
}

+ (ChromeIdentity*)fakeIdentity2 {
  return [FakeChromeIdentity identityWithEmail:@"foo2@gmail.com"
                                        gaiaID:@"foo2ID"
                                          name:@"Fake Foo 2"];
}

+ (ChromeIdentity*)fakeManagedIdentity {
  return [FakeChromeIdentity identityWithEmail:@"foo@managed.com"
                                        gaiaID:@"fooManagedID"
                                          name:@"Fake Managed"];
}

+ (NSError*)checkSignedInWithIdentity:(ChromeIdentity*)identity {
  if (identity == nil) {
    return testing::NSErrorWithLocalizedDescription(
        @"Need to give an identity");
  }

  // Required to avoid any problem since the following test is not dependant
  // to UI, and the previous action has to be totally finished before going
  // through the assert.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  CoreAccountInfo info =
      IdentityManagerFactory::GetForBrowserState(browser_state)
          ->GetPrimaryAccountInfo();

  if (base::SysNSStringToUTF8(identity.gaiaID) != info.gaia) {
    NSString* errorStr =
        [NSString stringWithFormat:
                      @"Unexpected Gaia ID of the signed in user [expected = "
                      @"\"%@\", actual = \"%s\"]",
                      identity.gaiaID, info.gaia.c_str()];
    return testing::NSErrorWithLocalizedDescription(errorStr);
  }

  return nil;
}

+ (NSError*)checkSignedOut {
  // Required to avoid any problem since the following test is not dependant to
  // UI, and the previous action has to be totally finished before going through
  // the assert.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  ios::ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();

  if (IdentityManagerFactory::GetForBrowserState(browser_state)
          ->HasPrimaryAccount()) {
    return testing::NSErrorWithLocalizedDescription(
        @"Unexpected signed in user");
  }

  return nil;
}

@end
