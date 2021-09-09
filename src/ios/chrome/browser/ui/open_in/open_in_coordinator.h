// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// Coordinator for OpenIn.
@interface OpenInCoordinator : ChromeCoordinator

// Disables all registered openInControllers.
- (void)disableAll;

@end

#endif  // IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_COORDINATOR_H_
