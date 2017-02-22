// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_DOMAIN_USER_INFO_H_
#define REMOTING_CLIENT_IOS_DOMAIN_USER_INFO_H_

#import <Foundation/Foundation.h>

// A detail record for a Remoting User.
// TODO(nicholss): This is not the final object yet.
@interface UserInfo : NSObject

@property(nonatomic, copy) NSString* userId;
@property(nonatomic, copy) NSString* userFullName;
@property(nonatomic, copy) NSString* userEmail;

+ (User*)parseListFromJSON:(NSMutableData*)data;

- (NSComparisonResult)compare:(UserInfo*)user;

@end

#endif  //  REMOTING_CLIENT_IOS_DOMAIN_USER_INFO_H_
