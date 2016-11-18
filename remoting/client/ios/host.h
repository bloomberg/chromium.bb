// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_HOST_H_
#define REMOTING_CLIENT_IOS_HOST_H_

#import <Foundation/Foundation.h>

// A detail record for a Chromoting Host.
@interface Host : NSObject

// Various properties of the Chromoting Host.
@property(nonatomic, copy) NSString* createdTime;
@property(nonatomic, copy) NSString* hostId;
@property(nonatomic, copy) NSString* hostName;
@property(nonatomic, copy) NSString* hostVersion;
@property(nonatomic, copy) NSString* jabberId;
@property(nonatomic, copy) NSString* kind;
@property(nonatomic, copy) NSString* publicKey;
@property(nonatomic, copy) NSString* status;
@property(nonatomic, copy) NSDate* updatedTime;
// True when |status| is @"ONLINE", anything else is False.
@property(nonatomic, readonly) bool isOnline;

+ (NSMutableArray*)parseListFromJSON:(NSMutableData*)data;

// First consider if |isOnline| is greater than anything else, then consider by
// case insensitive locale of |hostName|.
- (NSComparisonResult)compare:(Host*)host;

@end

#endif  //  REMOTING_CLIENT_IOS_HOST_H_
