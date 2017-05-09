// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_FACADE_REMOTING_SERVICE_H_
#define REMOTING_CLIENT_IOS_FACADE_REMOTING_SERVICE_H_

#import "remoting/client/chromoting_client_runtime.h"

#include "base/memory/weak_ptr.h"
#include "remoting/base/oauth_token_getter.h"

@class HostInfo;
@class UserInfo;
@class RemotingAuthentication;

// Eventing related keys:

// Hosts did update event.
extern NSString* const kHostsDidUpdate;
// User did update event name.
extern NSString* const kUserDidUpdate;
// Map key for UserInfo object.
extern NSString* const kUserInfo;

// |RemotingService| is the centralized place to ask for information about
// authentication or query the remote services. It also helps deal with the
// runtime and threading used in the application. |RemotingService| is a
// singleton and should only be accessed via the |SharedInstance| method.
@interface RemotingService : NSObject

// Access to the singleton shared instance from this method.
+ (RemotingService*)SharedInstance;

// Start a request to fetch the host list. This will produce an notification on
// |kHostsDidUpdate| when a new host is ready.
- (void)requestHostListFetch;

@property(nonatomic, readonly) RemotingAuthentication* authentication;

// Returns the current host list.
@property(nonatomic, readonly) NSArray<HostInfo*>* hosts;

// The Chromoting Client Runtime, this holds the threads and other shared
// resources used by the Chromoting clients
@property(nonatomic, readonly) remoting::ChromotingClientRuntime* runtime;

@end

#endif  // REMOTING_CLIENT_IOS_FACADE_REMOTING_SERVICE_H_
