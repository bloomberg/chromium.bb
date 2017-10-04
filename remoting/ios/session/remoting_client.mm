// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/session/remoting_client.h"

#include <memory>

#import "base/mac/bind_objc_block.h"
#import "ios/third_party/material_components_ios/src/components/Dialogs/src/MaterialDialogs.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "remoting/ios/audio/audio_player_ios.h"
#import "remoting/ios/display/gl_display_handler.h"
#import "remoting/ios/domain/client_session_details.h"
#import "remoting/ios/domain/host_info.h"
#import "remoting/ios/keychain_wrapper.h"

#include "base/strings/sys_string_conversions.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/chromoting_session.h"
#include "remoting/client/connect_to_host_info.h"
#include "remoting/client/display/renderer_proxy.h"
#include "remoting/client/gesture_interpreter.h"
#include "remoting/client/input/keyboard_interpreter.h"
#include "remoting/ios/session/remoting_client_session_delegate.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/video_renderer.h"

NSString* const kHostSessionStatusChanged = @"kHostSessionStatusChanged";
NSString* const kHostSessionPinProvided = @"kHostSessionPinProvided";

NSString* const kSessionDetails = @"kSessionDetails";
NSString* const kSessionSupportsPairing = @"kSessionSupportsPairing";
NSString* const kSessonStateErrorCode = @"kSessonStateErrorCode";

NSString* const kHostSessionCreatePairing = @"kHostSessionCreatePairing";
NSString* const kHostSessionHostName = @"kHostSessionHostName";
NSString* const kHostSessionPin = @"kHostSessionPin";

@interface RemotingClient () {
  remoting::ChromotingClientRuntime* _runtime;
  std::unique_ptr<remoting::RemotingClientSessonDelegate> _sessonDelegate;
  ClientSessionDetails* _sessionDetails;
  // Call _secretFetchedCallback on the network thread.
  remoting::protocol::SecretFetchedCallback _secretFetchedCallback;
  std::unique_ptr<remoting::RendererProxy> _renderer;
  std::unique_ptr<remoting::GestureInterpreter> _gestureInterpreter;
  std::unique_ptr<remoting::KeyboardInterpreter> _keyboardInterpreter;
  std::unique_ptr<remoting::AudioPlayerIos> _audioPlayer;
  std::unique_ptr<remoting::ChromotingSession> _session;
}
@end

@implementation RemotingClient

@synthesize displayHandler = _displayHandler;

- (instancetype)init {
  self = [super init];
  if (self) {
    _runtime = remoting::ChromotingClientRuntime::GetInstance();
    _sessonDelegate.reset(new remoting::RemotingClientSessonDelegate(self));
    _sessionDetails = [[ClientSessionDetails alloc] init];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(hostSessionPinProvided:)
               name:kHostSessionPinProvided
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [self disconnectFromHost];
}

- (void)connectToHost:(HostInfo*)hostInfo
             username:(NSString*)username
          accessToken:(NSString*)accessToken {
  DCHECK(_runtime->ui_task_runner()->BelongsToCurrentThread());
  DCHECK(hostInfo);
  DCHECK(hostInfo.jabberId);
  DCHECK(hostInfo.hostId);
  DCHECK(hostInfo.publicKey);

  _sessionDetails.hostInfo = hostInfo;

  remoting::ConnectToHostInfo info;
  info.username = base::SysNSStringToUTF8(username);
  info.auth_token = base::SysNSStringToUTF8(accessToken);
  info.host_jid = base::SysNSStringToUTF8(hostInfo.jabberId);
  info.host_id = base::SysNSStringToUTF8(hostInfo.hostId);
  info.host_pubkey = base::SysNSStringToUTF8(hostInfo.publicKey);
  info.host_os = base::SysNSStringToUTF8(hostInfo.hostOs);
  info.host_os_version = base::SysNSStringToUTF8(hostInfo.hostOsVersion);
  info.host_version = base::SysNSStringToUTF8(hostInfo.hostVersion);

  NSDictionary* pairing =
      [KeychainWrapper.instance pairingCredentialsForHost:hostInfo.hostId];
  if (pairing) {
    info.pairing_id =
        base::SysNSStringToUTF8([pairing objectForKey:kKeychainPairingId]);
    info.pairing_secret =
        base::SysNSStringToUTF8([pairing objectForKey:kKeychainPairingSecret]);
  } else {
    info.pairing_id = "";
    info.pairing_secret = "";
  }

  // TODO(nicholss): I am not sure about the following fields yet.
  // info.capabilities =
  // info.flags =

  remoting::protocol::ClientAuthenticationConfig client_auth_config;
  client_auth_config.host_id = info.host_id;
  client_auth_config.pairing_client_id = info.pairing_id;
  client_auth_config.pairing_secret = info.pairing_secret;

  // ChromotingClient keeps strong reference to |client_auth_config| through its
  // lifetime.
  __weak RemotingClient* weakSelf = self;
  client_auth_config.fetch_secret_callback = base::BindBlockArc(
      ^(bool pairing_supported, const remoting::protocol::SecretFetchedCallback&
                                    secret_fetched_callback) {
        RemotingClient* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        strongSelf->_secretFetchedCallback = secret_fetched_callback;
        strongSelf->_sessionDetails.state = SessionPinPrompt;

        // Notification will be received on the thread they are posted, so we
        // need to post the notification on UI thread.
        strongSelf->_runtime->ui_task_runner()->PostTask(
            FROM_HERE, base::BindBlockArc(^() {
              [NSNotificationCenter.defaultCenter
                  postNotificationName:kHostSessionStatusChanged
                                object:weakSelf
                              userInfo:@{
                                kSessionDetails : strongSelf->_sessionDetails,
                                kSessionSupportsPairing :
                                    [NSNumber numberWithBool:pairing_supported],
                              }];
            }));
      });

  _audioPlayer = remoting::AudioPlayerIos::CreateAudioPlayer(
      _runtime->audio_task_runner());

  _displayHandler = [[GlDisplayHandler alloc] init];
  _displayHandler.delegate = self;

  _session.reset(new remoting::ChromotingSession(
      _sessonDelegate->GetWeakPtr(), [_displayHandler CreateCursorShapeStub],
      [_displayHandler CreateVideoRenderer],
      _audioPlayer->GetAudioStreamConsumer(), info, client_auth_config));
  _renderer = [_displayHandler CreateRendererProxy];
  _gestureInterpreter.reset(
      new remoting::GestureInterpreter(_renderer.get(), _session.get()));
  _keyboardInterpreter.reset(new remoting::KeyboardInterpreter(_session.get()));

  _session->Connect();
  _audioPlayer->Start();
}

- (void)disconnectFromHost {
  if (_session) {
    _session->Disconnect();
    _runtime->network_task_runner()->DeleteSoon(FROM_HERE, _session.release());
  }

  _displayHandler = nil;

  if (_audioPlayer) {
    _audioPlayer->Invalidate();
    _runtime->audio_task_runner()->DeleteSoon(FROM_HERE,
                                              _audioPlayer.release());
  }
  // This needs to be deleted on the display thread since GlDisplayHandler binds
  // its WeakPtrFactory to the display thread.
  // TODO(yuweih): Ideally this constraint can be removed once we allow
  // GlRenderer to be created on the UI thread before being used.
  if (_renderer) {
    _runtime->display_task_runner()->DeleteSoon(FROM_HERE, _renderer.release());
  }

  _gestureInterpreter.reset();
  _keyboardInterpreter.reset();
}

#pragma mark - Eventing

- (void)hostSessionPinProvided:(NSNotification*)notification {
  NSString* pin = [[notification userInfo] objectForKey:kHostSessionPin];
  NSString* name = UIDevice.currentDevice.name;
  BOOL createPairing = [[[notification userInfo]
      objectForKey:kHostSessionCreatePairing] boolValue];

  // TODO(nicholss): Look into refactoring ProvideSecret. It is mis-named and
  // does not use pin.
  if (_session) {
    _session->ProvideSecret(base::SysNSStringToUTF8(pin),
                            (createPairing == YES),
                            base::SysNSStringToUTF8(name));
  }

  if (_secretFetchedCallback) {
    remoting::protocol::SecretFetchedCallback callback = _secretFetchedCallback;
    _runtime->network_task_runner()->PostTask(
        FROM_HERE, base::BindBlockArc(^{
          callback.Run(base::SysNSStringToUTF8(pin));
        }));
    _secretFetchedCallback.Reset();
  }
}

#pragma mark - Properties

- (HostInfo*)hostInfo {
  return _sessionDetails.hostInfo;
}

- (remoting::GestureInterpreter*)gestureInterpreter {
  return _gestureInterpreter.get();
}

- (remoting::KeyboardInterpreter*)keyboardInterpreter {
  return _keyboardInterpreter.get();
}

#pragma mark - ChromotingSession::Delegate

- (void)onConnectionState:(remoting::protocol::ConnectionToHost::State)state
                    error:(remoting::protocol::ErrorCode)error {
  switch (state) {
    case remoting::protocol::ConnectionToHost::INITIALIZING:
      _sessionDetails.state = SessionInitializing;
      break;
    case remoting::protocol::ConnectionToHost::CONNECTING:
      _sessionDetails.state = SessionConnecting;
      break;
    case remoting::protocol::ConnectionToHost::AUTHENTICATED:
      _sessionDetails.state = SessionAuthenticated;
      break;
    case remoting::protocol::ConnectionToHost::CONNECTED:
      _sessionDetails.state = SessionConnected;
      break;
    case remoting::protocol::ConnectionToHost::FAILED:
      _sessionDetails.state = SessionFailed;
      break;
    case remoting::protocol::ConnectionToHost::CLOSED:
      _sessionDetails.state = SessionClosed;
      [self disconnectFromHost];
      break;
    default:
      LOG(ERROR) << "onConnectionState, unknown state: " << state;
  }

  switch (error) {
    case remoting::protocol::ErrorCode::OK:
      _sessionDetails.error = SessionErrorOk;
      break;
    case remoting::protocol::ErrorCode::PEER_IS_OFFLINE:
      _sessionDetails.error = SessionErrorPeerIsOffline;
      break;
    case remoting::protocol::ErrorCode::SESSION_REJECTED:
      _sessionDetails.error = SessionErrorSessionRejected;
      break;
    case remoting::protocol::ErrorCode::INCOMPATIBLE_PROTOCOL:
      _sessionDetails.error = SessionErrorIncompatibleProtocol;
      break;
    case remoting::protocol::ErrorCode::AUTHENTICATION_FAILED:
      _sessionDetails.error = SessionErrorAuthenticationFailed;
      break;
    case remoting::protocol::ErrorCode::INVALID_ACCOUNT:
      _sessionDetails.error = SessionErrorInvalidAccount;
      break;
    case remoting::protocol::ErrorCode::CHANNEL_CONNECTION_ERROR:
      _sessionDetails.error = SessionErrorChannelConnectionError;
      break;
    case remoting::protocol::ErrorCode::SIGNALING_ERROR:
      _sessionDetails.error = SessionErrorSignalingError;
      break;
    case remoting::protocol::ErrorCode::SIGNALING_TIMEOUT:
      _sessionDetails.error = SessionErrorSignalingTimeout;
      break;
    case remoting::protocol::ErrorCode::HOST_OVERLOAD:
      _sessionDetails.error = SessionErrorHostOverload;
      break;
    case remoting::protocol::ErrorCode::MAX_SESSION_LENGTH:
      _sessionDetails.error = SessionErrorMaxSessionLength;
      break;
    case remoting::protocol::ErrorCode::HOST_CONFIGURATION_ERROR:
      _sessionDetails.error = SessionErrorHostConfigurationError;
      break;
    default:
      _sessionDetails.error = SessionErrorUnknownError;
      break;
  }

  [[NSNotificationCenter defaultCenter]
      postNotificationName:kHostSessionStatusChanged
                    object:self
                  userInfo:[NSDictionary dictionaryWithObject:_sessionDetails
                                                       forKey:kSessionDetails]];
}

- (void)commitPairingCredentialsForHost:(NSString*)host
                                     id:(NSString*)id
                                 secret:(NSString*)secret {
  [KeychainWrapper.instance commitPairingCredentialsForHost:host
                                                         id:id
                                                     secret:secret];
}

- (void)fetchThirdPartyTokenForUrl:(NSString*)tokenUrl
                          clientId:(NSString*)clientId
                             scope:(NSString*)scope {
  // Not supported for iOS yet.
  _sessionDetails.state = SessionCancelled;
  [self disconnectFromHost];
  NSString* message = [NSString
      stringWithFormat:@"[ThirdPartyAuth] Unable to authenticate with %@.",
                       _sessionDetails.hostInfo.hostName];
  [MDCSnackbarManager showMessage:[MDCSnackbarMessage messageWithText:message]];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kHostSessionStatusChanged
                    object:self
                  userInfo:[NSDictionary dictionaryWithObject:_sessionDetails
                                                       forKey:kSessionDetails]];
}

- (void)setCapabilities:(NSString*)capabilities {
  NSLog(@"TODO(nicholss): implement this, setCapabilities. %@", capabilities);
}

- (void)handleExtensionMessageOfType:(NSString*)type
                             message:(NSString*)message {
  NSLog(@"TODO(nicholss): implement this, handleExtensionMessageOfType %@:%@.",
        type, message);
}

- (void)setHostResolution:(CGSize)dipsResolution scale:(int)scale {
  _session->SendClientResolution(dipsResolution.width, dipsResolution.height,
                                 scale);
}

#pragma mark - GlDisplayHandlerDelegate

- (void)canvasSizeChanged:(CGSize)size {
  if (_gestureInterpreter) {
    _gestureInterpreter->OnDesktopSizeChanged(size.width, size.height);
  }
}

- (void)rendererTicked {
  if (_gestureInterpreter) {
    _gestureInterpreter->ProcessAnimations();
  }
}

@end
