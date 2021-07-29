// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/first_run/first_run_screen_view_controller.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_consumer.h"

// Delegate of sign-in screen view controller.
@protocol
    SigninScreenViewControllerDelegate <FirstRunScreenViewControllerDelegate>

// Called when the user taps to see the account picker.
- (void)showAccountPickerFromPoint:(CGPoint)point;

@end

// View controller of sign-in screen.
@interface SigninScreenViewController
    : FirstRunScreenViewController <SigninScreenConsumer>

@property(nonatomic, weak) id<SigninScreenViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_VIEW_CONTROLLER_H_
