// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/session/client.h"

#import "base/mac/bind_objc_block.h"

#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/connect_to_host_info.h"

@interface RemotingClient () {
  GlDisplayHandler* _displayHandler;
  remoting::ChromotingClientRuntime* _runtime;
  std::unique_ptr<remoting::ChromotingSession> _session;
  remoting::RemotingClientSessonDelegate* _sessonDelegate;
}
@end

@implementation RemotingClient

- (instancetype)init {
  self = [super init];
  if (self) {
    _runtime = ChromotingClientRuntime::GetInstance();
    _sessonDelegate = new remoting::RemotingClientSessonDelegate(self);
  }
  return self;
}

- (void)connectToHost:(const remoting::ConnectToHostInfo&)info {
  _displayHandler = [[GlDisplayHandler alloc] initWithRuntime:_runtime];

  protocol::ClientAuthenticationConfig client_auth_config;
  client_auth_config.host_id = info.host_id;
  client_auth_config.pairing_client_id = info.pairing_id;
  client_auth_config.pairing_secret = info.pairing_secret;
  client_auth_config.fetch_secret_callback = base::BindBlockArc(
      ^(bool pairing_supported,
        const SecretFetchedCallback& secret_fetched_callback) {
        NSLog(@"TODO(nicholss): Implement the FetchSecretCallback.");
      });

  // TODO(nicholss): Add audio support to iOS.
  base::WeakPtr<protocol::AudioStub> audioPlayer = nullptr;

  _session.reset(new remoting::ChromotingSession(
      _sessonDelegate->GetWeakPtr(), [_displayHandler CreateCursorShapeStub],
      [_displayHandler CreateVideoRenderer], audioPlayer, info,
      client_auth_config));
  _session->Connect();
}

#pragma mark - ChromotingSession::Delegate

- (void)onConnectionState:(protocol::ConnectionToHost::State)state
                    error:(protocol::ErrorCode)error {
  NSLog(@"TODO(nicholss): implement this.");
}

- (void)commitPairingCredentialsForHost:(NSString*)host
                                     id:(NSString*)id
                                 secret:(NSString*)secret {
  NSLog(@"TODO(nicholss): implement this.");
}

- (void)fetchThirdPartyTokenForUrl:(NSString*)tokenUrl
                          clientId:(NSString*)clientId
                             scope:(NSString*)scope {
  NSLog(@"TODO(nicholss): implement this.");
}

- (void)setCapabilities:(NSString*)capabilities {
  NSLog(@"TODO(nicholss): implement this.");
}

- (void)handleExtensionMessageOfType:(NSString*)type
                             message:(NSString*)message {
  NSLog(@"TODO(nicholss): implement this.");
}

@end
