// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_FACADE_REMOTING_SERVICE_H_
#define REMOTING_CLIENT_IOS_FACADE_REMOTING_SERVICE_H_

#import "remoting/client/chromoting_client_runtime.h"
#import "remoting/client/ios/domain/host_info.h"
#import "remoting/client/ios/domain/user_info.h"

#include "base/memory/weak_ptr.h"

// |RemotingAuthenticationDelegate|s are interested in authentication related
// notifications.
@protocol RemotingAuthenticationDelegate<NSObject>

// Notifies the delegate that the authentication status of the current user has
// changed to a new state.
- (void)nowAuthenticated:(BOOL)authenticated;

@end

// |RemotingHostListDelegate|s are interested in notifications related to host
// list.
@protocol RemotingHostListDelegate<NSObject>

- (void)hostListUpdated;

@end

// |RemotingService| is the centralized place to ask for information about
// authentication or query the remote services. It also helps deal with the
// runtime and threading used in the application. |RemotingService| is a
// singleton and should only be accessed via the |SharedInstance| method.
@interface RemotingService : NSObject

// Access to the singleton shared instance from this method.
+ (RemotingService*)SharedInstance;

// Access to the current |ChromotingClientRuntime| from this method.
- (remoting::ChromotingClientRuntime*)runtime;

// Register to be a |RemotingAuthenticationDelegate|.
- (void)setAuthenticationDelegate:(id<RemotingAuthenticationDelegate>)delegate;

// A cached answer if there is a currently authenticated user.
- (BOOL)isAuthenticated;

// Provide an |authorizationCode| to authenticate a user as the first time user
// of the application or OAuth Flow.
- (void)authenticateWithAuthorizationCode:(NSString*)authorizationCode;

// Provide the |refreshToken| and |email| to authenticate a user as a returning
// user of the application.
- (void)authenticateWithRefreshToken:(NSString*)refreshToken
                               email:(NSString*)email;

// Returns the currently logged in user info from cache, or nil if no
// currently authenticated user.
- (UserInfo*)getUser;

// Register to be a |RemotingHostListDelegate|. Side effect of setting this
// delegate is the application will attempt to fetch a fresh host list.
- (void)setHostListDelegate:(id<RemotingHostListDelegate>)delegate;

// Returns the currently cached host list or nil if none exist.
- (NSArray<HostInfo*>*)getHosts;

@end

#endif  // REMOTING_CLIENT_IOS_FACADE_REMOTING_SERVICE_H_
