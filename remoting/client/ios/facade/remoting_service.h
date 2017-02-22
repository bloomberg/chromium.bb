// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_FACADE_REMOTING_SERCICE_H_
#define REMOTING_CLIENT_IOS_FACADE_REMOTING_SERCICE_H_

#import "remoting/client/ios/domain/host_info.h"
#import "remoting/client/ios/domain/user_info.h"

@interface RemotingService : NSObject

+ (RemotingService*)sharedInstance;

- (BOOL)isAuthenticated;
- (void)authenticateWithAuthorizationCode:(NSString*)authorizationCode;
- (UserInfo*)getUser;
- (NSArray<HostInfo*>*)getHosts;

@end

#endif  // REMOTING_CLIENT_IOS_FACADE_REMOTING_SERCICE_H_
