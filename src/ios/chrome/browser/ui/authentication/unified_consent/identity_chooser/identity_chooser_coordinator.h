// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class ChromeIdentity;
@protocol IdentityChooserCoordinatorDelegate;

// Coordinator to display the identity chooser view controller.
@interface IdentityChooserCoordinator : ChromeCoordinator

// Selected ChromeIdentity.
@property(nonatomic, strong) ChromeIdentity* selectedIdentity;
// Delegate.
@property(nonatomic, weak) id<IdentityChooserCoordinatorDelegate> delegate;
// Origin of the animation for the IdentityChooser.
@property(nonatomic, assign) CGPoint origin;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_COORDINATOR_H_
