// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils_app_interface.h"

#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_cell.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SigninEarlGreyUtilsAppInterface

+ (FakeChromeIdentity*)fakeIdentity1 {
  return [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                        gaiaID:@"foo1ID"
                                          name:@"Fake Foo 1"];
}

+ (FakeChromeIdentity*)fakeIdentity2 {
  return [FakeChromeIdentity identityWithEmail:@"foo2@gmail.com"
                                        gaiaID:@"foo2ID"
                                          name:@"Fake Foo 2"];
}

+ (void)addFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      fakeIdentity);
}

+ (void)forgetFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->ForgetIdentity(fakeIdentity, nil);
}

+ (NSString*)primaryAccountGaiaID {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  CoreAccountInfo info =
      IdentityManagerFactory::GetForBrowserState(browser_state)
          ->GetPrimaryAccountInfo();

  return base::SysUTF8ToNSString(info.gaia);
}

+ (BOOL)isSignedOut {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();

  return !IdentityManagerFactory::GetForBrowserState(browser_state)
              ->HasPrimaryAccount();
}

+ (id<GREYMatcher>)identityCellMatcherForEmail:(NSString*)email {
  return grey_allOf(grey_accessibilityID(email),
                    grey_kindOfClass([IdentityChooserCell class]),
                    grey_sufficientlyVisible(), nil);
}

+ (void)removeFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->RemoveIdentity(fakeIdentity);
}

+ (NSUInteger)bookmarkCount:(NSString*)title {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());

  base::string16 matchString = base::SysNSStringToUTF16(title);
  std::vector<bookmarks::TitledUrlMatch> matches;
  bookmarkModel->GetBookmarksMatching(matchString, 50, &matches);
  const size_t count = matches.size();
  return count;
}

+ (BOOL)isAuthenticated {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  return authentication_service->IsAuthenticated();
}

+ (void)signOut {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  authentication_service->SignOut(signin_metrics::SIGNOUT_TEST,
                                  /*force_clear_browsing_data=*/false, nil);
}

+ (void)addBookmark:(NSString*)urlString withTitle:(NSString*)title {
  GURL bookmarkURL = GURL(base::SysNSStringToUTF8(urlString));
  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  bookmark_model->AddURL(bookmark_model->mobile_node(), 0,
                         base::SysNSStringToUTF16(title), bookmarkURL);
}

@end
