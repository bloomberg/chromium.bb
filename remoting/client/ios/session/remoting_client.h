// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_SESSION_REMOTING_CLIENT_H_
#define REMOTING_CLIENT_IOS_SESSION_REMOTING_CLIENT_H_

#import <Foundation/Foundation.h>

#include "remoting/protocol/connection_to_host.h"

@class HostInfo;
@class GlDisplayHandler;

// A list of notifications that will be sent out for different types of Remoting
// Client events.
//
extern NSString* const kHostSessionStatusChanged;
extern NSString* const kHostSessionPinProvided;

// List of keys in user info from events.
extern NSString* const kSessionDetails;
extern NSString* const kSessonStateErrorCode;
extern NSString* const kHostSessionPin;

// Remoting Client is the entry point for starting a session with a remote
// host. This object should not be reused. Remoting Client will use the default
// NSNotificationCenter to signal session state changes using the key
// |kHostSessionStatusChanged|. It expects to receive an event back on
// |kHostSessionPinProvided| when the session is asking for a PIN authenication.
@interface RemotingClient : NSObject

// Connect to a given host.
// |hostInfo| is all the details around a host.
// |username| is the username to be used when connecting.
// |accessToken| is the oAuth access token to provided to create the session.
- (void)connectToHost:(HostInfo*)hostInfo
             username:(NSString*)username
          accessToken:(NSString*)accessToken;

// Mirrors the native client session delegate interface:

- (void)onConnectionState:(remoting::protocol::ConnectionToHost::State)state
                    error:(remoting::protocol::ErrorCode)error;

- (void)commitPairingCredentialsForHost:(NSString*)host
                                     id:(NSString*)id
                                 secret:(NSString*)secret;

- (void)fetchThirdPartyTokenForUrl:(NSString*)tokenUrl
                          clientId:(NSString*)clinetId
                             scope:(NSString*)scope;

- (void)setCapabilities:(NSString*)capabilities;

- (void)handleExtensionMessageOfType:(NSString*)type message:(NSString*)message;

// The display handler tied to the remoting client used to display the host.
@property(nonatomic, strong) GlDisplayHandler* displayHandler;
// The host info used to make the remoting client connection.
@property(nonatomic, readonly) HostInfo* hostInfo;

@end

#endif  // REMOTING_CLIENT_IOS_SESSION_REMOTING_CLIENT_H_
