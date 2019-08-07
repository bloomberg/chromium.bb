// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_PUBLIC_SIGNIN_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_PUBLIC_SIGNIN_PRESENTER_H_

@class ShowSigninCommand;

// Protocol used to display signin UI.
@protocol SigninPresenter
// Asks the presenter to display the signin UI configured by |command|.
- (void)showSignin:(ShowSigninCommand*)command;
@end

#endif  // IOS_CHROME_BROWSER_UI_SIGNIN_INTERACTION_PUBLIC_SIGNIN_PRESENTER_H_
