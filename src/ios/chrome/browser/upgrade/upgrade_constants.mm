// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/upgrade/upgrade_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kIOSChromeNextVersionKey = @"UpdateInfobarUpgradeURL";
NSString* const kIOSChromeUpgradeURLKey = @"UpdateInfobarNextVersion";
NSString* const kIOSChromeUpToDateKey = @"UpdateInfobarIsUpToDate";
NSString* const kLastInfobarDisplayTimeKey = @"UpdateInfobarLastDisplayTime";
const NSTimeInterval kInfobarDisplayIntervalInSeconds =
    24 * 60 * 60;  // One day.
