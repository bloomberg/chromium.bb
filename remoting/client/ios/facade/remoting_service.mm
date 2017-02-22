// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <Foundation/Foundation.h>

#import "remoting/client/ios/facade/remoting_service.h"

#include "base/logging.h"

@interface RemotingService ()

@property(nonatomic, copy) NSString* authorizationCode;

@end

//
// RemodingService will act as the facade to the C++ layer that has not been
// implemented/integrated yet.
// TODO(nicholss): Implement/Integrate this class. At the moment it is being
// used to generate fake data to implement the UI of the app.
//
@implementation RemotingService

@synthesize authorizationCode = _authorizationCode;

- (BOOL)isAuthenticated {
  return self.authorizationCode != nil;
}

- (void)authenticateWithAuthorizationCode:(NSString*)authorizationCode {
  self.authorizationCode = authorizationCode;
}

- (UserInfo*)getUser {
  if (![self isAuthenticated]) {
    return nil;
  }

  NSMutableString* json = [[NSMutableString alloc] init];
  [json appendString:@"{"];
  [json appendString:@"\"userId\":\"AABBCC123\","];
  [json appendString:@"\"userFullName\":\"John Smith\","];
  [json appendString:@"\"userEmail\":\"john@example.com\","];
  [json appendString:@"}"];

  NSMutableData* data = [NSMutableData
      dataWithData:[[json copy] dataUsingEncoding:NSUTF8StringEncoding]];

  UserInfo* user = [UserInfo parseListFromJSON:data];
  return user;
}

- (NSArray<HostInfo*>*)getHosts {
  if (![self isAuthenticated]) {
    return nil;
  }

  NSMutableString* json = [[NSMutableString alloc] init];
  [json
      appendString:@"{\"data\":{\"kind\":\"chromoting#hostList\",\"items\":["];
  [json appendString:@"{"];
  [json appendString:@"\"createdTime\":\"2000-01-01T00:00:01.000Z\","];
  [json appendString:@"\"hostId\":\"Host1\","];
  [json appendString:@"\"hostName\":\"HostName1\","];
  [json appendString:@"\"hostVersion\":\"2.22.5.4\","];
  [json appendString:@"\"kind\":\"Chromoting#host\","];
  [json appendString:@"\"jabberId\":\"JabberingOn\","];
  [json appendString:@"\"publicKey\":\"AAAAABBBBBZZZZZ\","];
  [json appendString:@"\"status\":\"TESTING\","];
  [json appendString:@"\"updatedTime\":\"2000-01-01T00:00:01.000Z\""];
  [json appendString:@"},"];
  [json appendString:@"{"];
  [json appendString:@"\"createdTime\":\"2000-01-01T00:00:01.000Z\","];
  [json appendString:@"\"hostId\":\"Host2\","];
  [json appendString:@"\"hostName\":\"HostName2\","];
  [json appendString:@"\"hostVersion\":\"2.22.5.4\","];
  [json appendString:@"\"kind\":\"Chromoting#host\","];
  [json appendString:@"\"jabberId\":\"JabberingOn\","];
  [json appendString:@"\"publicKey\":\"AAAAABBBBBZZZZZ\","];
  [json appendString:@"\"status\":\"ONLINE\","];
  [json appendString:@"\"updatedTime\":\"2000-01-01T00:00:01.000Z\""];
  [json appendString:@"}"];
  [json appendString:@"]}}"];

  NSMutableData* data = [NSMutableData
      dataWithData:[[json copy] dataUsingEncoding:NSUTF8StringEncoding]];

  NSMutableArray<HostInfo*>* hosts = [HostInfo parseListFromJSON:data];
  return hosts;
}

// RemotingService is a singleton.
+ (RemotingService*)sharedInstance {
  static RemotingService* sharedInstance = nil;
  static dispatch_once_t guard;
  dispatch_once(&guard, ^{
    sharedInstance = [[RemotingService alloc] init];
  });
  return sharedInstance;
}

@end
