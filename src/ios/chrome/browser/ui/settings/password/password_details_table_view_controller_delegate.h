// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

namespace autofill {
struct PasswordForm;
}  // namespace autofill

@class PasswordDetailsTableViewController;

// PasswordDetailsTableViewController uses this protocal to interact with higher
// level password controller.
@protocol PasswordDetailsTableViewControllerDelegate

- (void)passwordDetailsTableViewController:
            (PasswordDetailsTableViewController*)controller
                            deletePassword:
                                (const autofill::PasswordForm&)passwordForm;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
