// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/client_connection_view_controller.h"

#import "base/mac/bind_objc_block.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MDCActivityIndicator.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/NavigationBar/src/MaterialNavigationBar.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "remoting/ios/app/host_view_controller.h"
#import "remoting/ios/app/pin_entry_view.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/session_reconnect_view.h"
#import "remoting/ios/domain/client_session_details.h"
#import "remoting/ios/domain/host_info.h"
#import "remoting/ios/facade/remoting_authentication.h"
#import "remoting/ios/facade/remoting_service.h"
#import "remoting/ios/session/remoting_client.h"

#include "base/strings/sys_string_conversions.h"
#include "remoting/protocol/client_authentication_config.h"

static const CGFloat kIconRadius = 30.f;
static const CGFloat kActivityIndicatorStrokeWidth = 3.f;
static const CGFloat kActivityIndicatorRadius = 33.f;

static const CGFloat kPinEntryViewWidth = 240.f;
static const CGFloat kPinEntryViewHeight = 90.f;

static const CGFloat kReconnectViewWidth = 120.f;
static const CGFloat kReconnectViewHeight = 90.f;

static const CGFloat kTopPadding = 240.f;
static const CGFloat kPadding = 20.f;
static const CGFloat kMargin = 20.f;

static const CGFloat kBarHeight = 58.f;

static const CGFloat kKeyboardAnimationTime = 0.3;

@interface ClientConnectionViewController ()<PinEntryDelegate,
                                             SessionReconnectViewDelegate> {
  UIImageView* _iconView;
  MDCActivityIndicator* _activityIndicator;
  NSLayoutConstraint* _activityIndicatorTopConstraintFull;
  NSLayoutConstraint* _activityIndicatorTopConstraintKeyboard;
  UILabel* _statusLabel;
  MDCNavigationBar* _navBar;
  PinEntryView* _pinEntryView;
  SessionReconnectView* _reconnectView;
  NSString* _remoteHostName;
  RemotingClient* _client;
  SessionErrorCode _lastError;
  HostInfo* _hostInfo;
}
@end

@implementation ClientConnectionViewController

@synthesize state = _state;

- (instancetype)initWithHostInfo:(HostInfo*)hostInfo {
  self = [super init];
  if (self) {
    _hostInfo = hostInfo;
    _remoteHostName = hostInfo.hostName;

    // TODO(yuweih): This logic may be reused by other views.
    UIButton* cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [cancelButton setTitle:@"CANCEL" forState:UIControlStateNormal];
    [cancelButton
        setImage:[RemotingTheme
                         .backIcon imageFlippedForRightToLeftLayoutDirection]
        forState:UIControlStateNormal];
    [cancelButton addTarget:self
                     action:@selector(didTapCancel:)
           forControlEvents:UIControlEventTouchUpInside];
    self.navigationItem.leftBarButtonItem =
        [[UIBarButtonItem alloc] initWithCustomView:cancelButton];

    _navBar = [[MDCNavigationBar alloc] initWithFrame:CGRectZero];
    [_navBar observeNavigationItem:self.navigationItem];

    [_navBar setBackgroundColor:RemotingTheme.connectionViewBackgroundColor];
    MDCNavigationBarTextColorAccessibilityMutator* mutator =
        [[MDCNavigationBarTextColorAccessibilityMutator alloc] init];
    [mutator mutate:_navBar];
    [self.view addSubview:_navBar];
    _navBar.translatesAutoresizingMaskIntoConstraints = NO;

    // Attach navBar to the top of the view.
    [NSLayoutConstraint activateConstraints:@[
      [[_navBar topAnchor] constraintEqualToAnchor:[self.view topAnchor]],
      [[_navBar leadingAnchor]
          constraintEqualToAnchor:[self.view leadingAnchor]],
      [[_navBar trailingAnchor]
          constraintEqualToAnchor:[self.view trailingAnchor]],
      [[_navBar heightAnchor] constraintEqualToConstant:kBarHeight],
    ]];

    [self attemptConnectionToHost];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = RemotingTheme.connectionViewBackgroundColor;

  _activityIndicator = [[MDCActivityIndicator alloc] initWithFrame:CGRectZero];
  _activityIndicator.radius = kActivityIndicatorRadius;
  _activityIndicator.trackEnabled = YES;
  _activityIndicator.strokeWidth = kActivityIndicatorStrokeWidth;
  _activityIndicator.cycleColors = @[ UIColor.whiteColor ];
  _activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_activityIndicator];

  _statusLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  _statusLabel.numberOfLines = 1;
  _statusLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _statusLabel.textColor = [UIColor whiteColor];
  _statusLabel.textAlignment = NSTextAlignmentCenter;
  _statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_statusLabel];

  _iconView = [[UIImageView alloc] initWithFrame:CGRectZero];
  _iconView.contentMode = UIViewContentModeCenter;
  _iconView.alpha = 0.87f;
  _iconView.backgroundColor = RemotingTheme.onlineHostColor;
  _iconView.layer.cornerRadius = kIconRadius;
  _iconView.layer.masksToBounds = YES;
  _iconView.image = RemotingTheme.desktopIcon;
  _iconView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_iconView];

  _reconnectView = [[SessionReconnectView alloc] initWithFrame:CGRectZero];
  _reconnectView.hidden = YES;
  _reconnectView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_reconnectView];
  _reconnectView.delegate = self;

  _pinEntryView = [[PinEntryView alloc] init];
  _pinEntryView.hidden = YES;
  _pinEntryView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_pinEntryView];
  _pinEntryView.delegate = self;

  _reconnectView.hidden = YES;

  [self
      initializeLayoutConstraintsWithViews:NSDictionaryOfVariableBindings(
                                               _activityIndicator, _statusLabel,
                                               _iconView, _reconnectView,
                                               _pinEntryView)];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(hostSessionStatusChanged:)
             name:kHostSessionStatusChanged
           object:nil];
}

- (void)initializeLayoutConstraintsWithViews:(NSDictionary*)views {
  // Metrics to use in visual format strings.
  NSDictionary* layoutMetrics = @{
    @"padding" : @(kPadding),
    @"margin" : @(kMargin),
    @"topPadding" : @(kTopPadding),
    @"iconDiameter" : @(kIconRadius * 2),
    @"pinEntryViewWidth" : @(kPinEntryViewWidth),
    @"pinEntryViewHeight" : @(kPinEntryViewHeight),
    @"reconnectViewWidth" : @(kReconnectViewWidth),
    @"reconnectViewHeight" : @(kReconnectViewHeight),
  };
  [_activityIndicator sizeToFit];
  NSString* f;

  // Horizontal constraints:
  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:@"H:[_iconView(iconDiameter)]"
                                         options:0
                                         metrics:layoutMetrics
                                           views:views]];

  [self.view addConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:
                                    @"H:|-margin-[_statusLabel]-margin-|"
                                                    options:0
                                                    metrics:layoutMetrics
                                                      views:views]];

  [self.view addConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:
                                    @"H:[_pinEntryView(pinEntryViewWidth)]"
                                                    options:0
                                                    metrics:layoutMetrics
                                                      views:views]];

  [self.view addConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:
                                    @"H:[_reconnectView(reconnectViewWidth)]"
                                                    options:0
                                                    metrics:layoutMetrics
                                                      views:views]];

  // Anchors:
  _activityIndicatorTopConstraintFull =
      [_activityIndicator.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                                   constant:kTopPadding];
  _activityIndicatorTopConstraintFull.active = YES;

  [_iconView.centerYAnchor
      constraintEqualToAnchor:_activityIndicator.centerYAnchor]
      .active = YES;

  // Vertical constraints:
  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:@"V:[_iconView(iconDiameter)]"
                                         options:0
                                         metrics:layoutMetrics
                                           views:views]];

  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:
                         @"V:[_activityIndicator]-(padding)-[_statusLabel]"
                                         options:NSLayoutFormatAlignAllCenterX
                                         metrics:layoutMetrics
                                           views:views]];

  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:
                         @"V:[_iconView]-(padding)-[_statusLabel]"
                                         options:NSLayoutFormatAlignAllCenterX
                                         metrics:layoutMetrics
                                           views:views]];

  f = @"V:[_statusLabel]-(padding)-[_pinEntryView(pinEntryViewHeight)]";
  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:f
                                         options:NSLayoutFormatAlignAllCenterX
                                         metrics:layoutMetrics
                                           views:views]];

  f = @"V:[_statusLabel]-padding-[_reconnectView(reconnectViewHeight)]";
  [self.view addConstraints:
                 [NSLayoutConstraint
                     constraintsWithVisualFormat:f
                                         options:NSLayoutFormatAlignAllCenterX
                                         metrics:layoutMetrics
                                           views:views]];

  [self.view setNeedsUpdateConstraints];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.navigationController setNavigationBarHidden:YES animated:animated];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillShow:)
             name:UIKeyboardWillShowNotification
           object:nil];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillHide:)
             name:UIKeyboardWillHideNotification
           object:nil];

  [_activityIndicator startAnimating];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  [_activityIndicator stopAnimating];
}

- (BOOL)prefersStatusBarHidden {
  return YES;
}

#pragma mark - Keyboard

// TODO(nicholss): We need to listen to screen rotation and re-adjust the
// topAnchor.

- (void)keyboardWillShow:(NSNotification*)notification {
  CGSize keyboardSize =
      [[[notification userInfo] objectForKey:UIKeyboardFrameEndUserInfoKey]
          CGRectValue]
          .size;

  CGFloat newHeight = self.view.frame.size.height - keyboardSize.height;
  CGFloat overlap = newHeight - (_pinEntryView.frame.origin.y +
                                 _pinEntryView.frame.size.height + kPadding);
  if (overlap > 0) {
    overlap = 0;
  }
  _activityIndicatorTopConstraintKeyboard.active = NO;
  _activityIndicatorTopConstraintKeyboard = [_activityIndicator.topAnchor
      constraintEqualToAnchor:self.view.topAnchor
                     constant:kTopPadding + overlap];
  _activityIndicatorTopConstraintFull.active = NO;
  _activityIndicatorTopConstraintKeyboard.active = YES;
  [UIView animateWithDuration:kKeyboardAnimationTime
                   animations:^{
                     [self.view layoutIfNeeded];
                   }];
}

- (void)keyboardWillHide:(NSNotification*)notification {
  _activityIndicatorTopConstraintKeyboard.active = NO;
  _activityIndicatorTopConstraintFull.active = YES;
  [UIView animateWithDuration:kKeyboardAnimationTime
                   animations:^{
                     [self.view layoutIfNeeded];
                   }];
}

#pragma mark - Properties

- (void)setState:(ClientConnectionViewState)state {
  _state = state;
  switch (_state) {
    case ClientViewConnecting:
      [self showConnectingState];
      break;
    case ClientViewPinPrompt:
      [self showPinPromptState];
      break;
    case ClientViewConnected:
      [self showConnectedState];
      break;
    case ClientViewReconnect:
      [self showReconnect];
      break;
    case ClientViewClosed:
      [self.navigationController popToRootViewControllerAnimated:YES];
      break;
    case ClientViewError:
      [self showError];
      break;
  }
}

#pragma mark - SessionReconnectViewDelegate

- (void)didTapReconnect {
  [self attemptConnectionToHost];
}

#pragma mark - Private

- (void)attemptConnectionToHost {
  _client = [[RemotingClient alloc] init];
  __weak RemotingClient* weakClient = _client;
  __weak HostInfo* weakHostInfo = _hostInfo;
  [RemotingService.instance.authentication
      callbackWithAccessToken:^(RemotingAuthenticationStatus status,
                                NSString* userEmail, NSString* accessToken) {
        [weakClient connectToHost:weakHostInfo
                         username:userEmail
                      accessToken:accessToken];
      }];
  [self setState:ClientViewConnecting];
}

- (void)showConnectingState {
  [_pinEntryView endEditing:YES];
  _statusLabel.text =
      [NSString stringWithFormat:@"Connecting to %@", _remoteHostName];

  _pinEntryView.hidden = YES;

  _reconnectView.hidden = YES;

  [_activityIndicator stopAnimating];
  _activityIndicator.cycleColors = @[ [UIColor whiteColor] ];
  _activityIndicator.indicatorMode = MDCActivityIndicatorModeIndeterminate;
  _activityIndicator.hidden = NO;
  [_activityIndicator startAnimating];
}

- (void)showPinPromptState {
  _statusLabel.text = [NSString stringWithFormat:@"%@", _remoteHostName];
  [_activityIndicator stopAnimating];
  _activityIndicator.hidden = YES;

  _pinEntryView.hidden = NO;
  _reconnectView.hidden = YES;

  _reconnectView.hidden = YES;

  // TODO(yuweih): This may be called before viewDidAppear and miss the keyboard
  // callback.
  [_pinEntryView becomeFirstResponder];
}

- (void)showConnectedState {
  [_pinEntryView endEditing:YES];
  _statusLabel.text =
      [NSString stringWithFormat:@"Connected to %@", _remoteHostName];

  _pinEntryView.hidden = YES;
  [_pinEntryView clearPinEntry];

  _activityIndicator.progress = 0.0;
  _activityIndicator.hidden = NO;
  _activityIndicator.indicatorMode = MDCActivityIndicatorModeDeterminate;
  _activityIndicator.cycleColors = @[ [UIColor greenColor] ];
  [_activityIndicator startAnimating];
  _activityIndicator.progress = 1.0;
  _reconnectView.hidden = YES;

  _reconnectView.hidden = YES;

  HostViewController* hostViewController =
      [[HostViewController alloc] initWithClient:_client];
  _client = nil;

  [self.navigationController pushViewController:hostViewController animated:NO];
}

- (void)showReconnect {
  _statusLabel.text =
      [NSString stringWithFormat:@"Connection closed for %@", _remoteHostName];
  [_activityIndicator stopAnimating];
  _activityIndicator.hidden = YES;

  _pinEntryView.hidden = YES;

  _reconnectView.hidden = NO;

  [self.navigationController popToViewController:self animated:YES];
  [MDCSnackbarManager
      showMessage:[MDCSnackbarMessage messageWithText:@"Connection Closed."]];
}

- (void)showError {
  _statusLabel.text =
      [NSString stringWithFormat:@"Error connecting to %@", _remoteHostName];

  _pinEntryView.hidden = YES;

  _activityIndicator.indicatorMode = MDCActivityIndicatorModeDeterminate;
  _activityIndicator.cycleColors = @[ [UIColor redColor] ];
  _activityIndicator.progress = 1.0;
  _activityIndicator.hidden = NO;
  [_activityIndicator startAnimating];

  _reconnectView.hidden = NO;

  MDCSnackbarMessage* message = nil;
  switch (_lastError) {
    case SessionErrorOk:
      // Do nothing.
      break;
    case SessionErrorPeerIsOffline:
      message = [MDCSnackbarMessage
          messageWithText:@"Error: SessionErrorPeerIsOffline."];
      break;
    case SessionErrorSessionRejected:
      message = [MDCSnackbarMessage
          messageWithText:@"Error: SessionErrorSessionRejected."];
      break;
    case SessionErrorIncompatibleProtocol:
      message = [MDCSnackbarMessage
          messageWithText:@"Error: SessionErrorIncompatibleProtocol."];
      break;
    case SessionErrorAuthenticationFailed:
      message = [MDCSnackbarMessage messageWithText:@"Error: Invalid Pin."];
      [_pinEntryView clearPinEntry];
      break;
    case SessionErrorInvalidAccount:
      message = [MDCSnackbarMessage
          messageWithText:@"Error: SessionErrorInvalidAccount."];
      break;
    case SessionErrorChannelConnectionError:
      message = [MDCSnackbarMessage
          messageWithText:@"Error: SessionErrorChannelConnectionError."];
      break;
    case SessionErrorSignalingError:
      message = [MDCSnackbarMessage
          messageWithText:@"Error: SessionErrorSignalingError."];
      break;
    case SessionErrorSignalingTimeout:
      message = [MDCSnackbarMessage
          messageWithText:@"Error: SessionErrorSignalingTimeout."];
      break;
    case SessionErrorHostOverload:
      message = [MDCSnackbarMessage
          messageWithText:@"Error: SessionErrorHostOverload."];
      break;
    case SessionErrorMaxSessionLength:
      message = [MDCSnackbarMessage
          messageWithText:@"Error: SessionErrorMaxSessionLength."];
      break;
    case SessionErrorHostConfigurationError:
      message = [MDCSnackbarMessage
          messageWithText:@"Error: SessionErrorHostConfigurationError."];
      break;
    case SessionErrorUnknownError:
      message = [MDCSnackbarMessage
          messageWithText:@"Error: SessionErrorUnknownError."];
      break;
  }
  if (message.text) {
    [MDCSnackbarManager showMessage:message];
  }
}

- (void)didProvidePin:(NSString*)pin createPairing:(BOOL)createPairing {
  // TODO(nicholss): There is an open question if createPairing is supported on
  // iOS. Need to fingure this out.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kHostSessionPinProvided
                    object:self
                  userInfo:[NSDictionary dictionaryWithObject:pin
                                                       forKey:kHostSessionPin]];
}

- (void)didTapCancel:(id)sender {
  _client = nil;
  [self.navigationController popViewControllerAnimated:YES];
}

- (void)hostSessionStatusChanged:(NSNotification*)notification {
  NSLog(@"hostSessionStatusChanged: %@", [notification userInfo]);
  ClientConnectionViewState state;
  ClientSessionDetails* sessionDetails =
      [[notification userInfo] objectForKey:kSessionDetails];
  switch (sessionDetails.state) {
    case SessionInitializing:
    // Same as HostConnecting in UI. Fall-though.
    case SessionAuthenticated:
    // Same as HostConnecting in UI. Fall-though.
    case SessionConnecting:
      state = ClientViewConnecting;
      break;
    case SessionPinPrompt:
      state = ClientViewPinPrompt;
      break;
    case SessionConnected:
      state = ClientViewConnected;
      break;
    case SessionFailed:
      state = ClientViewError;
      break;
    case SessionClosed:
      // If the session closes, offer the user to reconnect.
      state = ClientViewReconnect;
      break;
    default:
      LOG(ERROR) << "Unknown State for Session, " << sessionDetails.state;
      return;
  }
  _lastError = sessionDetails.error;
  [[NSOperationQueue mainQueue] addOperationWithBlock:^{
    [self setState:state];
  }];
}

@end
