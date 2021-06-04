// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

namespace password_manager {
struct PasswordForm;
}

// Delegate for |PasswordsTableViewController|.
@protocol PasswordsTableViewControllerDelegate

// Deletes form with its duplicates.
- (void)deletePasswordForms:
    (const std::vector<password_manager::PasswordForm>&)forms;

// Starts password check.
- (void)startPasswordCheck;

// Returns string containing the timestamp of the last password check. If the
// check finished less than 1 minute ago string will look "Last check just
// now.", otherwise "Last check X minutes/hours... ago.". If check never run
// string will be "Check never run.".
- (NSString*)formatElapsedTimeSinceLastCheck;

// Returns detailed information about Password Check error if applicable.
- (NSAttributedString*)passwordCheckErrorInfo;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
