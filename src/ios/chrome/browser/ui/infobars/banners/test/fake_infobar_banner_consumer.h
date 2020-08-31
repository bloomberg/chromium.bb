// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_TEST_FAKE_INFOBAR_BANNER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_TEST_FAKE_INFOBAR_BANNER_CONSUMER_H_

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"

// Fake InfobarBannerConsumer used in tests.
@interface FakeInfobarBannerConsumer : NSObject <InfobarBannerConsumer>
// Redefine InfobarBannerConsumer properties as readwrite.
@property(nonatomic, copy) NSString* bannerAccessibilityLabel;
@property(nonatomic, copy) NSString* buttonText;
@property(nonatomic, strong) UIImage* iconImage;
@property(nonatomic, assign) BOOL presentsModal;
@property(nonatomic, copy) NSString* titleText;
@property(nonatomic, copy) NSString* subtitleText;
@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_TEST_FAKE_INFOBAR_BANNER_CONSUMER_H_
