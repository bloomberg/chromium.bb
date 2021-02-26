// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/password/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol PasswordIssuesPresenter;

// Screen with a list of compromised credentials.
@interface PasswordIssuesTableViewController
    : SettingsRootTableViewController <PasswordIssuesConsumer>

@property(nonatomic, weak) id<PasswordIssuesPresenter> presenter;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_TABLE_VIEW_CONTROLLER_H_
