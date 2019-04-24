// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COMMAND_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COMMAND_HANDLER_H_

#import <UIKit/UIKit.h>

// Protocol to communicate user actions from the mediator to its coordinator.
@protocol ManageSyncSettingsCommandHandler <NSObject>

// Opens the passphrase dialog.
- (void)openPassphraseDialog;
// Opens the "Web & App Activity" dialog.
- (void)openWebAppActivityDialog;
// Opens the "Data from Chrome sync" web page.
- (void)openDataFromChromeSyncWebPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_COMMAND_HANDLER_H_
