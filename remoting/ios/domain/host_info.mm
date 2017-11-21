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
