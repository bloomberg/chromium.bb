// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_PASSWORD_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_PASSWORD_TEST_UTIL_H_

#import "ios/chrome/browser/ui/settings/password/reauthentication_module.h"

@interface MockReauthenticationModule : NSObject<ReauthenticationProtocol>

// Localized string containing the reason why reauthentication is requested.
@property(nonatomic, copy) NSString* localizedReasonForAuthentication;

// Indicates whether the device is capable of reauthenticating the user.
@property(nonatomic, assign) BOOL canAttempt;

// Indicates whether (mock) authentication should succeed or not. Setting
// |shouldSucceed| to any value sets |canAttempt| to YES.
@property(nonatomic, assign) BOOL shouldSucceed;

@end

namespace chrome_test_util {

// Replace the reauthentication module in
// PasswordDetailsCollectionViewController with a fake one to avoid being
// blocked with a reauth prompt, and return the fake reauthentication module.
MockReauthenticationModule* SetUpAndReturnMockReauthenticationModule();

// Replace the reauthentication module in
// PasswordExporter with a fake one to avoid being
// blocked with a reauth prompt, and return the fake reauthentication module.
MockReauthenticationModule* SetUpAndReturnMockReauthenticationModuleForExport();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_PASSWORD_TEST_UTIL_H_
