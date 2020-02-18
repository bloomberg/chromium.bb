// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_SETTINGS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_SETTINGS_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// EarlGreyScopedBlockSwizzlerAppInterface contains the app-side
// implementation for helpers. These helpers are compiled into
// the app binary and can be called from either app or test code.
@interface PasswordSettingsAppInterface : NSObject

// Sets a re-authentication mock (i.e. what asks user for fingerprint to
// view password) and its options for next test.
+ (void)setUpMockReauthenticationModule;
+ (void)setUpMockReauthenticationModuleForExport;
+ (void)mockReauthenticationModuleShouldSucceed:(BOOL)shouldSucceed;
+ (void)mockReauthenticationModuleCanAttempt:(BOOL)canAttempt;

// Dismisses snack bar.  Used before next test.
+ (void)dismissSnackBar;

// Removes all credentials stored.
+ (BOOL)clearPasswordStore;

// Creates multiple password form with index being part of the username,
// password, origin and realm.
+ (void)saveExamplePasswordWithCount:(NSInteger)count;

// Creates password form for given fields.
+ (BOOL)saveExamplePassword:(NSString*)password
                   userName:(NSString*)userName
                     origin:(NSString*)origin;

// Creates a blocked password form for given origin.
+ (BOOL)saveExampleBlockedOrigin:(NSString*)origin;

// Creates a federated password form for given origins and user.
+ (BOOL)saveExampleFederatedOrigin:(NSString*)federatedOrigin
                          userName:(NSString*)userName
                            origin:(NSString*)origin;

// Gets number of password form stored.
+ (NSInteger)passwordStoreResultsCount;

// Returns YES is crdential service is enabled.
+ (BOOL)isCredentialsServiceEnabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORDS_SETTINGS_APP_INTERFACE_H_
