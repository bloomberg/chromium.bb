// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_PRESENTER_H_

#import <Foundation/Foundation.h>

// Object presenting the feature.
@protocol PasswordBreachPresenter <NSObject>

// Presents more information related to the feature.
- (void)presentLearnMore;

// Dismisses more information related to the feature.
- (void)dismissLearnMore;

// Informs the presenter that the feature should dismiss.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_PRESENTER_H_
