// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_H_

#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

// A fake ChromeIdentity used for testing.
@interface FakeChromeIdentity : ChromeIdentity <NSSecureCoding>

// Returns a ChromeIdentity based on |email|, |gaiaID| and |name|.
// The |hashedGaiaID| property will be derived from |name|.
// For simplicity, both |userGivenName| and |userFullName| properties use
// |name|.
+ (FakeChromeIdentity*)identityWithEmail:(NSString*)email
                                  gaiaID:(NSString*)gaiaID
                                    name:(NSString*)name;

// Redeclared as readwrite.
@property(strong, nonatomic, readwrite) NSString* userEmail;
@property(strong, nonatomic, readwrite) NSString* gaiaID;
@property(strong, nonatomic, readwrite) NSString* userFullName;
@property(strong, nonatomic, readwrite) NSString* userGivenName;
@property(strong, nonatomic, readwrite) NSString* hashedGaiaID;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_H_
