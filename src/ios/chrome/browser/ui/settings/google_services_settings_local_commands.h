// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_LOCAL_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_LOCAL_COMMANDS_H_

// Protocol to communicate GoogleServicesSettingsVC actions to its coordinator.
@protocol GoogleServicesSettingsLocalCommands<NSObject>

// Called when the "Google Activity Controls" dialog should be opened.
- (void)openGoogleActivityControlsDialog;
// Called when the "Encryption" dialog should be opened.
- (void)openEncryptionDialog;
// Called when the "Manage Synced Data" web page should be opened.
- (void)openManageSyncedDataWebPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_LOCAL_COMMANDS_H_
