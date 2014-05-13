// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/bridge/host_proxy.h"

#import "remoting/ios/data_store.h"
#import "remoting/ios/host_preferences.h"
#import "remoting/ios/bridge/client_instance.h"
#import "remoting/ios/bridge/client_proxy.h"

@implementation HostProxy

// Override default constructor and initialize internals
- (id)init {
  self = [super init];
  if (self) {
    _isConnected = false;
  }
  return self;
}

// Override default destructor
- (void)dealloc {
  if (_isConnected) {
    [self disconnectFromHost];
  }

  [super dealloc];
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

  NSString* pairId = @"";
  NSString* pairSecret = @"";

  const HostPreferences* hostPrefs =
      [[DataStore sharedStore] getHostForId:hostId];

  // Use the pairing id and secret when known
  if (hostPrefs && hostPrefs.pairId && hostPrefs.pairSecret) {
    pairId = [hostPrefs.pairId copy];
    pairSecret = [hostPrefs.pairSecret copy];
  }

  _hostToClientChannel.reset(new remoting::ClientProxy(
      [ClientProxyDelegateWrapper wrapDelegate:delegate]));

  DCHECK(!_clientToHostChannel);
  _clientToHostChannel =
      new remoting::ClientInstance(_hostToClientChannel->AsWeakPtr(),
                                   [username UTF8String],
                                   [token UTF8String],
                                   [jid UTF8String],
                                   [hostId UTF8String],
                                   [hostPublicKey UTF8String],
                                   [pairId UTF8String],
                                   [pairSecret UTF8String]);

  _clientToHostChannel->Start();
  _isConnected = YES;
}

- (void)authenticationResponse:(NSString*)pin createPair:(BOOL)createPair {
  if (_isConnected) {
    _clientToHostChannel->ProvideSecret([pin UTF8String], createPair);
  }
}

- (void)disconnectFromHost {
  if (_isConnected) {
    VLOG(1) << "Disconnecting from Host";

    // |_clientToHostChannel| must be closed before releasing
    // |_hostToClientChannel|

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
    _clientToHostChannel->PerformMouseAction(
        position, wheelDelta, buttonPressed, buttonIsDown);
  }
}

- (void)keyboardAction:(NSInteger)keyCode keyDown:(BOOL)keyIsDown {
  if (_isConnected) {
    _clientToHostChannel->PerformKeyboardAction(keyCode, keyIsDown);
  }
}

@end
