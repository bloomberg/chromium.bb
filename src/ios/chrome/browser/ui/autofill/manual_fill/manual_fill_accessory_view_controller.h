// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ACCESSORY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ACCESSORY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Protocol to handle user interactions in a ManualFillAccessoryViewController.
@protocol ManualFillAccessoryViewControllerDelegate

// Invoked after the user touches the `accounts` button.
- (void)accountButtonPressed;

// Invoked after the user touches the `credit cards` button.
- (void)cardButtonPressed;

// Invoked after the user touches the `keyboard` button.
- (void)keyboardButtonPressed;

// Invoked after the user touches the `passwords` button.
- (void)passwordButtonPressed;

@end

// This view contains the icons to activate "Manual Fill". It is meant to be
// shown above the keyboard on iPhone and above the manual fill view.
@interface ManualFillAccessoryViewController : UIViewController

// Instances an object with the desired delegate.
//
// @param delegate The delegate for this object.
// @return A fresh object with the passed delegate.
- (instancetype)initWithDelegate:
    (id<ManualFillAccessoryViewControllerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// Unavailable. Use `initWithDelegate:`.
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Resets to the original state, with the keyboard icon hidden and no icon
// selected.
- (void)reset;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_ACCESSORY_VIEW_CONTROLLER_H_
