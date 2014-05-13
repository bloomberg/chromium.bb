// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_DATA_STORE_H_
#define REMOTING_IOS_DATA_STORE_H_

#import <CoreData/CoreData.h>

#import "remoting/ios/host_preferences.h"

// A local data store backed by SQLLite to hold instances of HostPreferences.
// HostPreference is defined by the Core Data Model templates see
// ChromotingModel.xcdatamodel
@interface DataStore : NSObject

// Static pointer to the managed data store
+ (DataStore*)sharedStore;

// General methods
- (BOOL)saveChanges;

// Access methods for Hosts
- (NSArray*)allHosts;
- (const HostPreferences*)createHost:(NSString*)hostId;
- (void)removeHost:(const HostPreferences*)p;
- (const HostPreferences*)getHostForId:(NSString*)hostId;

@end

#endif  // REMOTING_IOS_DATA_STORE_H_
