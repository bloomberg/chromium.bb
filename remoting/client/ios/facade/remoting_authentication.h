// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_FACADE_REMOTING_AUTHENTICATION_H_
#define REMOTING_CLIENT_IOS_FACADE_REMOTING_AUTHENTICATION_H_

#import "remoting/client/chromoting_client_runtime.h"
#import "remoting/client/ios/domain/user_info.h"

#include "base/memory/weak_ptr.h"
#include "remoting/base/oauth_token_getter.h"

// |RemotingAuthenticationDelegate|s are interested in authentication related
// notifications.
@protocol RemotingAuthenticationDelegate<NSObject>

// Notifies the delegate that the user has been updated.
- (void)userDidUpdate:(UserInfo*)user;

@end

// This is the class that will manage the details around authentication
// management and currently active user. It will make sure the user object is
// saved to the keychain correctly and loaded on startup. It also is the entry
// point for gaining access to an auth token for authrized calls.
@interface RemotingAuthentication : NSObject

// Provide an |authorizationCode| to authenticate a user as the first time user
// of the application or OAuth Flow.
- (void)authenticateWithAuthorizationCode:(NSString*)authorizationCode;

// Fetches an OAuth Access Token and passes it back to the callback if
// the user is authenticated. Otherwise does nothing.
// TODO(nicholss): We might want to throw an error or add error message to
// the callback sig to be able to react to the un-authed case.
- (void)callbackWithAccessToken:
    (const remoting::OAuthTokenGetter::TokenCallback&)onAccessToken;

// Forget the current user.
- (void)logout;

// Returns the currently logged in user or nil.
@property(strong, nonatomic) UserInfo* user;

// Delegate recieves updates on user changes.
@property(weak, nonatomic) id<RemotingAuthenticationDelegate> delegate;

@end

#endif  // REMOTING_CLIENT_IOS_FACADE_REMOTING_AUTHENTICATION_H_
