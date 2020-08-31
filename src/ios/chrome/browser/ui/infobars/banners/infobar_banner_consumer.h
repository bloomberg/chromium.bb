// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol InfobarBannerConsumer <NSObject>

// Accessibility label for the banner view.  Default value is the concatenation
// of the title and subtitle texts.
- (void)setBannerAccessibilityLabel:(NSString*)bannerAccessibilityLabel;

// The button text displayed by this InfobarBanner.
- (void)setButtonText:(NSString*)buttonText;

// The icon displayed by this InfobarBanner.
- (void)setIconImage:(UIImage*)iconImage;

// YES if the banner should be able to present a Modal. Changing this property
// will immediately update the Banner UI that is related to triggering modal
// presentation.
- (void)setPresentsModal:(BOOL)presentsModal;

// The title displayed by this InfobarBanner.
- (void)setTitleText:(NSString*)titleText;

// The subtitle displayed by this InfobarBanner.
- (void)setSubtitleText:(NSString*)subtitleText;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_CONSUMER_H_
