// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

@class AuthenticationFlow;
@class ChromeIdentity;
class PrefService;
@protocol SigninScreenConsumer;
@protocol SigninScreenMediatorDelegate;

// Mediator that handles the sign-in operation.
@interface SigninScreenMediator : NSObject

// The designated initializer.
- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer for this mediator.
@property(nonatomic, weak) id<SigninScreenConsumer> consumer;

// Delegate.
@property(nonatomic, weak) id<SigninScreenMediatorDelegate> delegate;

// The identity currently selected.
@property(nonatomic, strong) ChromeIdentity* selectedIdentity;

// Whether an account has been added. Must be set externally.
@property(nonatomic, assign) BOOL addedAccount;

// Disconnect the mediator.
- (void)disconnect;

// Starts the sign in process, using |authenticationFlow|.
- (void)startSignInWithAuthenticationFlow:
    (AuthenticationFlow*)authenticationFlow;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SIGNIN_SIGNIN_SCREEN_MEDIATOR_H_
