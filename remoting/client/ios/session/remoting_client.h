// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_SESSION_REMOTING_CLIENT_H_
#define REMOTING_CLIENT_IOS_SESSION_REMOTING_CLIENT_H_

#import <Foundation/Foundation.h>

#import "remoting/client/ios/display/gl_display_handler.h"

#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/session.h"

namespace remoting {
struct ConnectToHostInfo;
}

@interface RemotingClient : NSObject

- (void)connectToHost:(const remoting::ConnectToHostInfo&)info;

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

@property(nonatomic, strong) GlDisplayHandler* displayHandler;

@end

#endif  // REMOTING_CLIENT_IOS_SESSION_REMOTING_CLIENT_H_
