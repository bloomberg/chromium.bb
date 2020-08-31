// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Test specific helpers for password_breach_egtest.mm.
@interface PasswordBreachAppInterface : NSObject

// Shows Password Breach with a default type of leak and URL.
+ (void)showPasswordBreach;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_APP_INTERFACE_H_
