// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/session/remoting_client.h"

#import "base/mac/bind_objc_block.h"

#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/chromoting_session.h"
#include "remoting/client/connect_to_host_info.h"
#include "remoting/client/ios/session/remoting_client_session_delegate.h"
#include "remoting/protocol/video_renderer.h"

@interface RemotingClient () {
  remoting::ChromotingClientRuntime* _runtime;
  std::unique_ptr<remoting::ChromotingSession> _session;
  remoting::RemotingClientSessonDelegate* _sessonDelegate;
}
@end

@implementation RemotingClient

@synthesize displayHandler = _displayHandler;

- (instancetype)init {
  self = [super init];
  if (self) {
    _runtime = remoting::ChromotingClientRuntime::GetInstance();
    _sessonDelegate = new remoting::RemotingClientSessonDelegate(self);
  }
  return self;
}

- (void)connectToHost:(const remoting::ConnectToHostInfo&)info {
  remoting::ConnectToHostInfo hostInfo(info);

  remoting::protocol::ClientAuthenticationConfig client_auth_config;
  client_auth_config.host_id = info.host_id;
  client_auth_config.pairing_client_id = info.pairing_id;
  client_auth_config.pairing_secret = info.pairing_secret;
  client_auth_config.fetch_secret_callback = base::BindBlockArc(
      ^(bool pairing_supported, const remoting::protocol::SecretFetchedCallback&
                                    secret_fetched_callback) {
        NSLog(@"TODO(nicholss): Implement the FetchSecretCallback.");
        // TODO(nicholss): For now we pass back a junk number.
        secret_fetched_callback.Run("000000");
      });

  // TODO(nicholss): Add audio support to iOS.
  base::WeakPtr<remoting::protocol::AudioStub> audioPlayer = nullptr;

  _displayHandler = [[GlDisplayHandler alloc] init];

  _runtime->ui_task_runner()->PostTask(
      FROM_HERE, base::BindBlockArc(^{
        _session.reset(new remoting::ChromotingSession(
            _sessonDelegate->GetWeakPtr(),
            [_displayHandler CreateCursorShapeStub],
            [_displayHandler CreateVideoRenderer], audioPlayer, hostInfo,
            client_auth_config));
        _session->Connect();
      }));
}

#pragma mark - ChromotingSession::Delegate

- (void)onConnectionState:(remoting::protocol::ConnectionToHost::State)state
                    error:(remoting::protocol::ErrorCode)error {
  NSLog(@"TODO(nicholss): implement this, onConnectionState: %d %d.", state,
        error);
}

- (void)commitPairingCredentialsForHost:(NSString*)host
                                     id:(NSString*)id
                                 secret:(NSString*)secret {
  NSLog(@"TODO(nicholss): implement this, commitPairingCredentialsForHost.");
}

- (void)fetchThirdPartyTokenForUrl:(NSString*)tokenUrl
                          clientId:(NSString*)clientId
                             scope:(NSString*)scope {
  NSLog(@"TODO(nicholss): implement this, fetchThirdPartyTokenForUrl.");
}

- (void)setCapabilities:(NSString*)capabilities {
  NSLog(@"TODO(nicholss): implement this, setCapabilities.");
}

- (void)handleExtensionMessageOfType:(NSString*)type
                             message:(NSString*)message {
  NSLog(@"TODO(nicholss): implement this, handleExtensionMessageOfType %@:%@.",
        type, message);
}

@end
