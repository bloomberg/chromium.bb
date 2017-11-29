// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_KEYCHAIN_WRAPPER_H_
#define REMOTING_IOS_KEYCHAIN_WRAPPER_H_

#import <Foundation/Foundation.h>

@class UserInfo;

extern NSString* const kKeychainPairingId;
extern NSString* const kKeychainPairingSecret;

typedef void (^PairingCredentialsCallback)(NSString* pairingId,
                                           NSString* secret);

// Class to abstract the details from how iOS wants to write to the keychain.
@interface KeychainWrapper : NSObject

// Save a refresh token to the keychain.
- (void)setRefreshToken:(NSString*)refreshToken;
// Get the refresh token from the keychain, if there is one.
- (NSString*)refreshToken;
// Save the pairing credentials for the given host id.
- (void)commitPairingCredentialsForHost:(NSString*)host
                                     id:(NSString*)pairingId
                                 secret:(NSString*)secret;
// Get the pairing credentials for the given host id.
- (NSDictionary*)pairingCredentialsForHost:(NSString*)host;
// Reset the keychain and the cache.
- (void)resetKeychainItem;

// Access to the singleton shared instance from this property.
@property(nonatomic, readonly, class) KeychainWrapper* instance;

@end

#endif  //  REMOTING_IOS_KEYCHAIN_WRAPPER_H_
