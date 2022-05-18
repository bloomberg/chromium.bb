// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAFE_BROWSING_SAFE_BROWSING_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SAFE_BROWSING_SAFE_BROWSING_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// Used as a delegate for SafeBrowsingTabHelper and makes initial call to open
// safe browsing settings from interstitial page.
@interface SafeBrowsingCoordinator : ChromeCoordinator

@end

#endif  // IOS_CHROME_BROWSER_UI_SAFE_BROWSING_SAFE_BROWSING_COORDINATOR_H_
