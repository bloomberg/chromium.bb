// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// The top-level owner of the feed top section.
@interface FeedTopSectionCoordinator : ChromeCoordinator

@property(nonatomic, readonly, strong) UIViewController* viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_TOP_SECTION_COORDINATOR_H_
