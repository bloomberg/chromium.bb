// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/host.h"

@implementation Host

@synthesize createdTime = _createdTime;
@synthesize hostId = _hostId;
@synthesize hostName = _hostName;
@synthesize hostVersion = _hostVersion;
@synthesize jabberId = _jabberId;
@synthesize kind = _kind;
@synthesize publicKey = _publicKey;
@synthesize status = _status;
@synthesize updatedTime = _updatedTime;

// Parse jsonData into Host list
+ (NSMutableArray*)parseListFromJSON:(NSMutableData*)data {
  NSError* error;

  NSDictionary* json = [NSJSONSerialization JSONObjectWithData:data
                                                       options:kNilOptions
                                                         error:&error];

  NSDictionary* dataDict = [json objectForKey:@"data"];

  NSArray* availableServers = [dataDict objectForKey:@"items"];

  NSMutableArray* serverList = [[NSMutableArray alloc] init];

  NSUInteger idx = 0;
  NSDictionary* svr;
  NSUInteger count = [availableServers count];

  while (idx < count) {
    svr = [availableServers objectAtIndex:idx++];
    Host* host = [[Host alloc] init];
    host.createdTime = [svr objectForKey:@"createdTime"];
    host.hostId = [svr objectForKey:@"hostId"];
    host.hostName = [svr objectForKey:@"hostName"];
    host.hostVersion = [svr objectForKey:@"hostVersion"];
    host.jabberId = [svr objectForKey:@"jabberId"];
    host.kind = [svr objectForKey:@"kind"];
    host.publicKey = [svr objectForKey:@"publicKey"];
    host.status = [svr objectForKey:@"status"];
    host.updatedTime = [svr objectForKey:@"updatedTime"];
    [serverList addObject:host];
  }

  return serverList;
}

@end
