// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/domain/host_info.h"

@implementation HostInfo

@synthesize createdTime = _createdTime;
@synthesize hostId = _hostId;
@synthesize hostName = _hostName;
@synthesize hostOs = _hostOs;
@synthesize hostOsVersion = _hostOsVersion;
@synthesize hostVersion = _hostVersion;
@synthesize jabberId = _jabberId;
@synthesize kind = _kind;
@synthesize publicKey = _publicKey;
@synthesize status = _status;
@synthesize updatedTime = _updatedTime;
@synthesize offlineReason = _offlineReason;

- (bool)isOnline {
  return (self.status && [self.status isEqualToString:@"ONLINE"]);
}

// Parse jsonData into Host list.
+ (NSMutableArray<HostInfo*>*)parseListFromJSON:(NSMutableData*)data {
  NSError* error;

  NSDictionary* json = [NSJSONSerialization JSONObjectWithData:data
                                                       options:kNilOptions
                                                         error:&error];
  NSDictionary* dataDict = [json objectForKey:@"data"];
  NSArray* availableHosts = [dataDict objectForKey:@"items"];
  NSMutableArray* hostList = [[NSMutableArray alloc] init];
  NSUInteger idx = 0;
  NSDictionary* svr;
  NSUInteger count = [availableHosts count];

  while (idx < count) {
    svr = [availableHosts objectAtIndex:idx++];
    HostInfo* host = [[HostInfo alloc] init];
    host.createdTime = [svr objectForKey:@"createdTime"];
    host.hostId = [svr objectForKey:@"hostId"];
    host.hostName = [svr objectForKey:@"hostName"];
    host.hostOs = [svr objectForKey:@"hostOs"];
    host.hostVersion = [svr objectForKey:@"hostVersion"];
    host.hostOsVersion = [svr objectForKey:@"hostOsVersion"];
    host.jabberId = [svr objectForKey:@"jabberId"];
    host.kind = [svr objectForKey:@"kind"];
    host.publicKey = [svr objectForKey:@"publicKey"];
    host.status = [svr objectForKey:@"status"];

    //    NSString* ISO8601DateString = [svr objectForKey:@"updatedTime"];
    //    if (ISO8601DateString != nil) {
    //      NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
    //      [dateFormatter setDateFormat:@"yyyy-MM-dd'T'HH:mm:ss.SSSz"];
    //      host.updatedTime = [dateFormatter dateFromString:ISO8601DateString];
    //    }

    [hostList addObject:host];
  }

  return hostList;
}

- (NSComparisonResult)compare:(HostInfo*)host {
  if (self.isOnline != host.isOnline) {
    return self.isOnline ? NSOrderedAscending : NSOrderedDescending;
  } else {
    return [self.hostName localizedCaseInsensitiveCompare:host.hostName];
  }
}

- (NSString*)description {
  return
      [NSString stringWithFormat:@"HostInfo: name=%@ status=%@ updatedTime= %@",
                                 _hostName, _status, _updatedTime];
}

@end
