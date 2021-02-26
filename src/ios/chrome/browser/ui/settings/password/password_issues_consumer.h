// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_CONSUMER_H_

#import "ios/chrome/browser/ui/settings/password/password_issue.h"

// Consumer for the Password Issues Screen.
@protocol PasswordIssuesConsumer <NSObject>

// Pass password issues to the consumer.
- (void)setPasswordIssues:(NSArray<id<PasswordIssue>>*)passwords;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_ISSUES_CONSUMER_H_
