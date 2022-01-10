// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_VIEW_CONTROLLER_DELEGATE_H_

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@class SigninSyncViewController;

// Delegate of sign-in screen view controller.
@protocol SigninSyncViewControllerDelegate <PromoStyleViewControllerDelegate>

// Called when the user taps to see the account picker.
- (void)signinSyncViewController:
            (SigninSyncViewController*)signinSyncViewController
      showAccountPickerFromPoint:(CGPoint)point;

// Called when the user taps to see sync settings.
- (void)signinSyncViewControllerDidTapOnSettings:
    (SigninSyncViewController*)signinSyncViewController;

// Adds consent string ID.
- (void)signinSyncViewController:
            (SigninSyncViewController*)signinSyncViewController
              addConsentStringID:(const int)stringID;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_VIEW_CONTROLLER_DELEGATE_H_
