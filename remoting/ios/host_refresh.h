// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_HOST_REFRESH_H_
#define REMOTING_IOS_HOST_REFRESH_H_

#import <Foundation/Foundation.h>

#import "GTMOAuth2Authentication.h"

// HostRefresh encapsulates a fetch of the Chromoting host list from
// the jabber service.

// Contract to handle the host list result of a Chromoting host list fetch.
@protocol HostRefreshDelegate<NSObject>

- (void)hostListRefresh:(NSArray*)hostList errorMessage:(NSString*)errorMessage;

@end

// Fetches the host list from the jabber service async.  Authenticates,
// and parses the results to provide to a HostListViewController
@interface HostRefresh : NSObject<NSURLConnectionDataDelegate>

// Store data read while the connection is active, and can be used after the
// connection has been discarded
@property(nonatomic, copy) NSMutableData* jsonData;
@property(nonatomic, copy) NSString* errorMessage;
@property(nonatomic, assign) id<HostRefreshDelegate> delegate;

- (void)refreshHostList:(GTMOAuth2Authentication*)authReq
               delegate:(id<HostRefreshDelegate>)delegate;

@end

#endif  // REMOTING_IOS_HOST_REFRESH_H_
