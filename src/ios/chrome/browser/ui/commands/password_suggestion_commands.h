// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_PASSWORD_SUGGESTION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_PASSWORD_SUGGESTION_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands related to suggesting strong passwords.
@protocol PasswordSuggestionCommands

// Shows the password suggestion view controller.
- (void)showPasswordSuggestion:(NSString*)passwordSuggestion
               decisionHandler:(void (^)(BOOL accept))decisionHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_PASSWORD_SUGGESTION_COMMANDS_H_
