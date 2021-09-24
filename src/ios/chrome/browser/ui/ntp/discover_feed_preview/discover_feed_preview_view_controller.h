// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_PREVIEW_DISCOVER_FEED_PREVIEW_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_PREVIEW_DISCOVER_FEED_PREVIEW_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/ntp/discover_feed_preview/discover_feed_preview_consumer.h"

// ViewController for the feed preview. It displays a loaded webState UIView.
@interface DiscoverFeedPreviewViewController
    : UIViewController <DiscoverFeedPreviewConsumer>

// Inits the view controller with the |webStateView| and the |origin| of the
// preview.
- (instancetype)initWithView:(UIView*)webStateView
                      origin:(NSString*)origin NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNAme
                         bundle:(NSBundle*)nibBundle NS_UNAVAILABLE;

// Resets the auto layout for preview.
- (void)resetAutoLayoutForPreview;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_DISCOVER_FEED_PREVIEW_DISCOVER_FEED_PREVIEW_VIEW_CONTROLLER_H_
