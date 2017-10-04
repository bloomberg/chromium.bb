// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_FACADE_REMOTING_SERVICE_H_
#define REMOTING_IOS_FACADE_REMOTING_SERVICE_H_

#import "remoting/client/chromoting_client_runtime.h"

@class HostInfo;
@class UserInfo;

@protocol RemotingAuthentication;

typedef NS_ENUM(NSInteger, HostListState) {
  // Nobody has requested a host list fetch since login or last failure.
  HostListStateNotFetched,

  // The host list is currently being fetched.
  HostListStateFetching,

  // The host list has been fetched.
  HostListStateFetched,
};

typedef NS_ENUM(NSInteger, HostListFetchFailureReason) {
  HostListFetchFailureReasonNoFailure,
  HostListFetchFailureReasonNetworkError,
  HostListFetchFailureReasonAuthError,
  HostListFetchFailureReasonUnknown,
};

// Eventing related keys:

// Host list fetch failed event.
extern NSString* const kHostListFetchDidFail;
// Map key for the host list fetch failure reason.
extern NSString* const kHostListFetchFailureReasonKey;
// Hosts did update event.
extern NSString* const kHostListStateDidChange;
// User did update event name.
extern NSString* const kUserDidUpdate;
// Map key for UserInfo object.
extern NSString* const kUserInfo;

// |RemotingService| is the centralized place to ask for information about
// authentication or query the remote services. It also helps deal with the
// runtime and threading used in the application. |RemotingService| is a
// singleton and should only be accessed via the |SharedInstance| method.
@interface RemotingService : NSObject

// Start a request to fetch the host list. This will produce an notification on
// |kHostsDidUpdate| when a new host is ready.
- (void)requestHostListFetch;

// Access to the singleton shared instance from this property.
@property(nonatomic, readonly, class) RemotingService* instance;

// Returns the current host list.
@property(nonatomic, readonly) NSArray<HostInfo*>* hosts;

@property(nonatomic, readonly) HostListState hostListState;

// The Chromoting Client Runtime, this holds the threads and other shared
// resources used by the Chromoting clients
@property(nonatomic, readonly) remoting::ChromotingClientRuntime* runtime;

// Returns the last failure reason when fetching the host list. Returns
// HostListFetchFailureReasonNoFailure when the host list has never been fetched
// or the last fetch has succeeded.
@property(nonatomic, readonly)
    HostListFetchFailureReason lastFetchFailureReason;

// This must be set immediately after the authentication object is created. It
// can only be set once.
@property(nonatomic) id<RemotingAuthentication> authentication;

@end

#endif  // REMOTING_IOS_FACADE_REMOTING_SERVICE_H_
