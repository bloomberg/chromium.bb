// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"

#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using SigninPromoViewTest = PlatformTest;

TEST_F(SigninPromoViewTest, ChromiumLogoImageLegacy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kUIRefreshPhase1);
  UIWindow* currentWindow = [[UIApplication sharedApplication] keyWindow];
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                       style:SigninPromoViewUILegacy];
  view.mode = SigninPromoViewModeColdState;
  [currentWindow.rootViewController.view addSubview:view];
  UIImage* chromiumLogo = view.imageView.image;
  EXPECT_NE(nil, chromiumLogo);
  view.mode = SigninPromoViewModeWarmState;
  UIImage* customImage = [[UIImage alloc] init];
  [view setProfileImage:customImage];
  EXPECT_NE(nil, view.imageView.image);
  // The image should has been changed from the logo.
  EXPECT_NE(chromiumLogo, view.imageView.image);
  // The image should be different than the one set, since a circular background
  // should have been added.
  EXPECT_NE(customImage, view.imageView.image);
}

TEST_F(SigninPromoViewTest, ChromiumLogoImageUIRefresh) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kUIRefreshPhase1);
  UIWindow* currentWindow = [[UIApplication sharedApplication] keyWindow];
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                       style:SigninPromoViewUIRefresh];
  view.mode = SigninPromoViewModeColdState;
  [currentWindow.rootViewController.view addSubview:view];
  UIImage* chromiumLogo = view.imageView.image;
  EXPECT_NE(nil, chromiumLogo);
  view.mode = SigninPromoViewModeWarmState;
  UIImage* customImage = [[UIImage alloc] init];
  [view setProfileImage:customImage];
  EXPECT_NE(nil, view.imageView.image);
  // The image should has been changed from the logo.
  EXPECT_NE(chromiumLogo, view.imageView.image);
  // The image should be different than the one set, since a circular background
  // should have been added.
  EXPECT_NE(customImage, view.imageView.image);
}

TEST_F(SigninPromoViewTest, SecondaryButtonVisibilityLegacy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kUIRefreshPhase1);
  UIWindow* currentWindow = [[UIApplication sharedApplication] keyWindow];
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                       style:SigninPromoViewUILegacy];
  view.mode = SigninPromoViewModeColdState;
  [currentWindow.rootViewController.view addSubview:view];
  EXPECT_TRUE(view.secondaryButton.hidden);
  view.mode = SigninPromoViewModeWarmState;
  EXPECT_FALSE(view.secondaryButton.hidden);
}

TEST_F(SigninPromoViewTest, SecondaryButtonVisibilityUIRefresh) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kUIRefreshPhase1);
  UIWindow* currentWindow = [[UIApplication sharedApplication] keyWindow];
  SigninPromoView* view =
      [[SigninPromoView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)
                                       style:SigninPromoViewUIRefresh];
  view.mode = SigninPromoViewModeColdState;
  [currentWindow.rootViewController.view addSubview:view];
  EXPECT_TRUE(view.secondaryButton.hidden);
  view.mode = SigninPromoViewModeWarmState;
  EXPECT_FALSE(view.secondaryButton.hidden);
}
