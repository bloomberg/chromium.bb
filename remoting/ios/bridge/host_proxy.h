// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_BRIDGE_CLIENT_PROXY_H_
#define REMOTING_IOS_BRIDGE_CLIENT_PROXY_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

#import "remoting/ios/bridge/client_proxy_delegate_wrapper.h"

namespace remoting {
class ClientInstance;
class ClientProxy;
}  // namespace remoting

// HostProxy is one channel of a bridge from the UI Application (CLIENT) and the
// common Chromoting protocol (HOST). HostProxy proxies message from the UI
// application to the host. The reverse channel, ClientProxy, is owned by the
// HostProxy to control deconstruction order, but is shared with the
// ClientInstance to perform work.

@interface HostProxy : NSObject {
 @private
  // Host to Client channel
  scoped_ptr<remoting::ClientProxy> _hostToClientChannel;
  // Client to Host channel, must be released before |_hostToClientChannel|
  scoped_refptr<remoting::ClientInstance> _clientToHostChannel;
  // Connection state
  BOOL _isConnected;
}

// TRUE when a connection has been established successfully.
- (BOOL)isConnected;

// Forwards credentials from CLIENT and to HOST and begins establishing a
// connection.
- (void)connectToHost:(NSString*)username
            authToken:(NSString*)token
             jabberId:(NSString*)jid
               hostId:(NSString*)hostId
            publicKey:(NSString*)hostPublicKey
             delegate:(id<ClientProxyDelegate>)delegate;

// Report from CLIENT with the user's PIN.
- (void)authenticationResponse:(NSString*)pin createPair:(BOOL)createPair;

// CLIENT initiated disconnection
- (void)disconnectFromHost;

// Report from CLIENT of mouse input
- (void)mouseAction:(const webrtc::DesktopVector&)position
         wheelDelta:(const webrtc::DesktopVector&)wheelDelta
        whichButton:(NSInteger)buttonPressed
         buttonDown:(BOOL)buttonIsDown;

// Report from CLIENT of keyboard input
- (void)keyboardAction:(NSInteger)keyCode keyDown:(BOOL)keyIsDown;

@end

#endif  // REMOTING_IOS_BRIDGE_CLIENT_PROXY_H_
