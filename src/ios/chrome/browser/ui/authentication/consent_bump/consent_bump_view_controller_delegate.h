// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@class ConsentBumpViewController;

// Protocol for the delegate of the ConsentBumpViewController.
@protocol ConsentBumpViewControllerDelegate<NSObject>

// Notifies the delegate that the primary button of the view controller has been
// pressed.
- (void)consentBumpViewControllerDidTapPrimaryButton:
    (ConsentBumpViewController*)consentBumpViewController;
// Notifies the delegate that the secondary button of the view controller has
// been pressed.
- (void)consentBumpViewControllerDidTapSecondaryButton:
    (ConsentBumpViewController*)consentBumpViewController;
// Notifies the delegate that the more button of the view controller has been
// pressed.
- (void)consentBumpViewControllerDidTapMoreButton:
    (ConsentBumpViewController*)consentBumpViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_VIEW_CONTROLLER_DELEGATE_H_
