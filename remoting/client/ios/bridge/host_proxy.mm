// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/client/ios/bridge/host_proxy.h"

#import "remoting/client/ios/host_preferences.h"
#import "remoting/client/ios/bridge/client_instance.h"
#import "remoting/client/ios/bridge/client_proxy.h"

#include "base/strings/sys_string_conversions.h"

@implementation HostProxy

// Override default constructor and initialize internals.
- (id)init {
  self = [super init];
  if (self) {
    _isConnected = false;
  }
  return self;
}

// Override default destructor.
- (void)dealloc {
  if (_isConnected) {
    [self disconnectFromHost];
  }

  //[super dealloc]; // TODO(nicholss): ARC forbids explicit message send of
  //'dealloc'
}

- (BOOL)isConnected {
  return _isConnected;
}

- (void)connectToHost:(NSString*)username
            authToken:(NSString*)token
             jabberId:(NSString*)jid
               hostId:(NSString*)hostId
            publicKey:(NSString*)hostPublicKey
             delegate:(id<ClientProxyDelegate>)delegate {
  // Implicitly, if currently connected, discard the connection and begin a new
  // connection.
  [self disconnectFromHost];

  _hostToClientChannel.reset(new remoting::ClientProxy(
      [ClientProxyDelegateWrapper wrapDelegate:delegate]));

  DCHECK(!_clientToHostChannel.get());
  _clientToHostChannel = new remoting::ClientInstance(
      _hostToClientChannel->AsWeakPtr(), base::SysNSStringToUTF8(username),
      base::SysNSStringToUTF8(token), base::SysNSStringToUTF8(jid),
      base::SysNSStringToUTF8(hostId), base::SysNSStringToUTF8(hostPublicKey));

  HostPreferences* host = [HostPreferences hostForId:hostId];
  _clientToHostChannel->Start(base::SysNSStringToUTF8(host.pairId),
                              base::SysNSStringToUTF8(host.pairSecret));
  _isConnected = YES;
}

- (void)authenticationResponse:(NSString*)pin
                 createPairing:(BOOL)createPairing {
  if (_isConnected) {
    // Where |deviceId| is first created doesn't matter, but it does have to be
    // from an Obj-C file.  Creating |deviceId| now, just before passing a copy
    // into a C++ interface.
    NSString* deviceId =
        [[[UIDevice currentDevice] identifierForVendor] UUIDString];

    _clientToHostChannel->ProvideSecret(base::SysNSStringToUTF8(pin),
                                        createPairing,
                                        base::SysNSStringToUTF8(deviceId));
  }
}

- (void)disconnectFromHost {
  if (_isConnected) {
    VLOG(1) << "Disconnecting from Host";

    // |_clientToHostChannel| must be closed before releasing
    // |_hostToClientChannel|.

    // |_clientToHostChannel| owns several objects that have references to
    // itself.  These objects need to be cleaned up before we can release
    // |_clientToHostChannel|.
    _clientToHostChannel->Cleanup();
    // All other references to |_clientToHostChannel| should now be free.  When
    // the next statement is executed the destructor is called automatically.
    _clientToHostChannel = NULL;

    _hostToClientChannel.reset();

    _isConnected = NO;
  }
}

- (void)mouseAction:(const webrtc::DesktopVector&)position
         wheelDelta:(const webrtc::DesktopVector&)wheelDelta
        whichButton:(NSInteger)buttonPressed
         buttonDown:(BOOL)buttonIsDown {
  if (_isConnected) {
    _clientToHostChannel->PerformMouseAction(position, wheelDelta,
        (remoting::protocol::MouseEvent_MouseButton) buttonPressed,
        buttonIsDown);
  }
}

- (void)keyboardAction:(NSInteger)keyCode keyDown:(BOOL)keyIsDown {
  if (_isConnected) {
    _clientToHostChannel->PerformKeyboardAction(keyCode, keyIsDown);
  }
}

@end
