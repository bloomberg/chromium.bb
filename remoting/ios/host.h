// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_HOST_H_
#define REMOTING_IOS_HOST_H_

#import <Foundation/Foundation.h>

// A detail record for a Chromoting Host
@interface Host : NSObject

// Various properties of the Chromoting Host
@property(nonatomic, copy) NSString* createdTime;
@property(nonatomic, copy) NSString* hostId;
@property(nonatomic, copy) NSString* hostName;
@property(nonatomic, copy) NSString* hostVersion;
@property(nonatomic, copy) NSString* jabberId;
@property(nonatomic, copy) NSString* kind;
@property(nonatomic, copy) NSString* publicKey;
@property(nonatomic, copy) NSString* status;
@property(nonatomic, copy) NSString* updatedTime;

+ (NSMutableArray*)parseListFromJSON:(NSMutableData*)data;

@end

#endif  //  REMOTING_IOS_HOST_H_
