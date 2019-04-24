// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/credential.h"

#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ManualFillCredentialiOSTest = PlatformTest;

// Tests that a credential is correctly created.
TEST_F(ManualFillCredentialiOSTest, Creation) {
  NSString* username = @"username@example.com";
  NSString* password = @"password";
  NSString* siteName = @"example.com";
  NSString* host = @"sub.example.com";
  ManualFillCredential* credential =
      [[ManualFillCredential alloc] initWithUsername:username
                                            password:password
                                            siteName:siteName
                                                host:host];
  EXPECT_TRUE(credential);
  EXPECT_TRUE([username isEqualToString:credential.username]);
  EXPECT_TRUE([password isEqualToString:credential.password]);
  EXPECT_TRUE([siteName isEqualToString:credential.siteName]);
  EXPECT_TRUE([host isEqualToString:credential.host]);
}

// Test equality between credentials.
TEST_F(ManualFillCredentialiOSTest, Equality) {
  NSString* username = @"username@example.com";
  NSString* password = @"password";
  NSString* siteName = @"example.com";
  NSString* host = @"sub.example.com";
  ManualFillCredential* credential =
      [[ManualFillCredential alloc] initWithUsername:username
                                            password:password
                                            siteName:siteName
                                                host:host];
  ManualFillCredential* equalCredential =
      [[ManualFillCredential alloc] initWithUsername:username
                                            password:password
                                            siteName:siteName
                                                host:host];
  EXPECT_TRUE([credential isEqual:equalCredential]);

  ManualFillCredential* differentUsernameCredential =
      [[ManualFillCredential alloc] initWithUsername:@"username2"
                                            password:password
                                            siteName:siteName
                                                host:host];
  EXPECT_FALSE([credential isEqual:differentUsernameCredential]);

  ManualFillCredential* differentPasswordCredential =
      [[ManualFillCredential alloc] initWithUsername:username
                                            password:@"psswd"
                                            siteName:siteName
                                                host:host];
  EXPECT_FALSE([credential isEqual:differentPasswordCredential]);

  ManualFillCredential* differentSiteNameCredential =
      [[ManualFillCredential alloc] initWithUsername:username
                                            password:password
                                            siteName:@"notexample.com"
                                                host:host];
  EXPECT_FALSE([credential isEqual:differentSiteNameCredential]);

  ManualFillCredential* differentHostCredential =
      [[ManualFillCredential alloc] initWithUsername:username
                                            password:password
                                            siteName:siteName
                                                host:@"other.example.com"];
  EXPECT_FALSE([credential isEqual:differentHostCredential]);
}
