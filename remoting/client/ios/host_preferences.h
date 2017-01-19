// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_HOST_PREFERENCES_H_
#define REMOTING_CLIENT_IOS_HOST_PREFERENCES_H_

#import <CoreData/CoreData.h>

// A HostPreferences contains details to negotiate and maintain a connection
// to a remote Chromoting host.  This is an entity in a backing store.
@interface HostPreferences : NSObject<NSCoding>

// Properties supplied by the host server.
@property(nonatomic, copy) NSString* hostId;
@property(nonatomic, copy) NSString* pairId;
@property(nonatomic, copy) NSString* pairSecret;

// Commit this record using the Keychain for current identity.
- (void)saveToKeychain;

// Load a record from the Keychain for current identity.
// If a record does not exist, return a new record with a blank secret.
+ (HostPreferences*)hostForId:(NSString*)hostId;

@end

#endif  // REMOTING_CLIENT_IOS_HOST_PREFERENCES_H_
