// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/domain/user_info.h"

@implementation UserInfo

@synthesize userId = _userId;
@synthesize userFullName = _userFullName;
@synthesize userEmail = _userEmail;
@synthesize refreshToken = _refreshToken;

// Parse jsonData into Host list.
+ (UserInfo*)parseListFromJSON:(NSMutableData*)data {
  NSError* error;

  NSDictionary* json = [NSJSONSerialization JSONObjectWithData:data
                                                       options:kNilOptions
                                                         error:&error];

  UserInfo* user = [[UserInfo alloc] init];
  user.userId = [json objectForKey:@"userId"];
  user.userFullName = [json objectForKey:@"userFullName"];
  user.userEmail = [json objectForKey:@"userEmail"];
  user.refreshToken = [json objectForKey:@"refreshToken"];

  return user;
}

- (BOOL)isAuthenticated {
  if (_userEmail && _userEmail.length > 0 && _refreshToken &&
      _refreshToken.length > 0) {
    return YES;
  }
  return NO;
}

- (NSComparisonResult)compare:(UserInfo*)user {
  return [self.userId compare:user.userId];
}

- (NSString*)description {
  return [NSString stringWithFormat:@"UserInfo: userEmail=%@ refreshToken=%@",
                                    _userEmail, _refreshToken];
}

@end
