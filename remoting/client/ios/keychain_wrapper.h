// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_KEYCHAIN_WRAPPER_H_
#define REMOTING_CLIENT_IOS_KEYCHAIN_WRAPPER_H_

#import <Foundation/Foundation.h>

@class UserInfo;

// Class to abstract the details from how iOS wants to write to the keychain.
// TODO(nicholss): This will have to be futher refactored when we integrate
// with the private Google auth.
@interface KeychainWrapper : NSObject

// Save a refresh token to the keychain.
- (void)setRefreshToken:(NSString*)refreshToken;
// Get the refresh token from the keychain, if there is one.
- (NSString*)refreshToken;
// Reset the keychain and the cache.
- (void)resetKeychainItem;

@end

#endif  //  REMOTING_CLIENT_IOS_KEYCHAIN_WRAPPER_H_
