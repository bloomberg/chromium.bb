// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_NAVIGATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_NAVIGATION_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class GoogleServicesNavigationCoordinator;

// GoogleServicesNavigationCoordinator delegate.
@protocol GoogleServicesNavigationCoordinatorDelegate<NSObject>

// Called when the user closed GoogleServicesNavigationCoordinator.
- (void)googleServicesNavigationCoordinatorDidClose:
    (GoogleServicesNavigationCoordinator*)coordinator;

@end

// Shows the Google services settings in a navigation controller.
@interface GoogleServicesNavigationCoordinator : ChromeCoordinator

// Delegate.
@property(nonatomic, weak) id<GoogleServicesNavigationCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_NAVIGATION_COORDINATOR_H_
