// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_CONSUMER_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <vector>

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

// Enum with all possible UI states of password check.
typedef NS_ENUM(NSInteger, PasswordCheckUIState) {
  // When no compromised passwords were detected.
  PasswordCheckStateSafe,
  // When user has compromised passwords.
  PasswordCheckStateUnSafe,
  // When check was not perfect and state is unclear.
  PasswordCheckStateDefault,
  // When password check is running.
  PasswordCheckStateRunning,
  // When user has no passwords and check can't be performed.
  PasswordCheckStateDisabled,
  // When password check failed due to network issues, quota limit or others.
  PasswordCheckStateError,
};

// Consumer for the Passwords Screen.
@protocol PasswordsConsumer <NSObject>

// Displays current password check UI state on screen.
- (void)setPasswordCheckUIState:(PasswordCheckUIState)state
      compromisedPasswordsCount:(NSInteger)count;

// Displays password and blocked forms.
- (void)
    setPasswordsForms:(std::vector<password_manager::PasswordForm>)savedForms
         blockedForms:(std::vector<password_manager::PasswordForm>)blockedForms;

// Updates "On/Off" state for Passwords In Other Apps item.
- (void)updatePasswordsInOtherAppsDetailedText;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_CONSUMER_H_
