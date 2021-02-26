// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_DELEGATE_H_

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

@class PasswordDetailsCoordinator;

// Delegate for PasswordIssuesCoordinator.
@protocol PasswordDetailsCoordinatorDelegate

// Called when the view controller was removed from navigation controller.
- (void)passwordDetailsCoordinatorDidRemove:
    (PasswordDetailsCoordinator*)coordinator;

// Called when user deleted password. This action should be handled
// outside to update the list of passwords immediately.
- (void)passwordDetailsCoordinator:(PasswordDetailsCoordinator*)coordinator
                    deletePassword:
                        (const password_manager::PasswordForm&)password;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_DETAILS_PASSWORD_DETAILS_COORDINATOR_DELEGATE_H_
