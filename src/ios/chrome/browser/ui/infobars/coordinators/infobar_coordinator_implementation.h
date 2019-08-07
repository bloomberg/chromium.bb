// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_IMPLEMENTATION_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_IMPLEMENTATION_H_

// Methods that need to be implemented by the InfobarCoordinator subclasses.
// TODO(crbug.com/945478): Assess if the InfobarDelegate can be owned by a
// mediator class once the implementation of the Password Infobar message is
// completed. This way we might not need different Coordinators for each
// Infobar, and we'll have different mediators instead.
@protocol InfobarCoordinatorImplementation

// Initializes and configures the ModalViewController that will be presented by
// the InfobarCoordinator. Returns YES if the modalViewController was configured
// successfully. If it returns NO no Modal should be presented.
- (BOOL)configureModalViewController;

// Performs any actions related to an Infobar Banner presentation.
- (void)infobarBannerWasPresented;

// Performs any actions related to an Infobar Modal presentation.
- (void)infobarModalPresentedFromBanner:(BOOL)presentedFromBanner;

// Dismisses the InfobarBanner once there's no interaction in progress. An
// interaction is any user initiated behavior with the Banner.
- (void)dismissBannerWhenInteractionIsFinished;

// Performs the main Infobar action. e.g. "Save Password", "Restore",etc.
- (void)performInfobarAction;

// Called after the Infobar (either Modal or Banner) has been dismissed.
// Transitioning from Banner to Modal won't call this method.
- (void)infobarWasDismissed;

// The infobar modal height. Used to calculate its presentation container
// height.
- (CGFloat)infobarModalHeightForWidth:(CGFloat)width;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_IMPLEMENTATION_H_
