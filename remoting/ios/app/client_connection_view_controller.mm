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
#import "remoting/ios/app/host_view_controller.h"
#import "remoting/ios/app/pin_entry_view.h"
#import "remoting/ios/app/remoting_theme.h"
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

static const CGFloat kCenterShift = -80.f;
static const CGFloat kPadding = 20.f;
static const CGFloat kMargin = 20.f;

static const CGFloat kBarHeight = 58.f;

static const CGFloat kKeyboardAnimationTime = 0.3;

@interface ClientConnectionViewController ()<PinEntryDelegate> {
  UIImageView* _iconView;
  MDCActivityIndicator* _activityIndicator;
  UILabel* _statusLabel;
  MDCNavigationBar* _navBar;
  PinEntryView* _pinEntryView;
  NSString* _remoteHostName;
  RemotingClient* _client;
}
@end

@implementation ClientConnectionViewController

@synthesize state = _state;

- (instancetype)initWithHostInfo:(HostInfo*)hostInfo {
  self = [super init];
  if (self) {
    _client = [[RemotingClient alloc] init];

    __weak RemotingClient* weakClient = _client;
    [[RemotingService SharedInstance].authentication
        callbackWithAccessToken:base::BindBlockArc(^(
                                    remoting::OAuthTokenGetter::Status status,
                                    const std::string& user_email,
                                    const std::string& access_token) {
          [weakClient connectToHost:hostInfo
                           username:base::SysUTF8ToNSString(user_email)
                        accessToken:base::SysUTF8ToNSString(access_token)];
        })];

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
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  [super loadView];

  self.view.backgroundColor = RemotingTheme.connectionViewBackgroundColor;

  _activityIndicator = [[MDCActivityIndicator alloc] initWithFrame:CGRectZero];
  [self.view addSubview:_activityIndicator];

  _statusLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  [self.view addSubview:_statusLabel];

  _iconView = [[UIImageView alloc] initWithFrame:CGRectZero];
  [self.view addSubview:_iconView];

  _pinEntryView = [[PinEntryView alloc] init];
  [self.view addSubview:_pinEntryView];
  _pinEntryView.delegate = self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  _iconView.contentMode = UIViewContentModeCenter;
  _iconView.alpha = 0.87f;
  _iconView.backgroundColor = RemotingTheme.onlineHostColor;
  _iconView.layer.cornerRadius = kIconRadius;
  _iconView.layer.masksToBounds = YES;
  _iconView.image = RemotingTheme.desktopIcon;

  _activityIndicator.radius = kActivityIndicatorRadius;
  _activityIndicator.trackEnabled = YES;
  _activityIndicator.strokeWidth = kActivityIndicatorStrokeWidth;
  _activityIndicator.cycleColors = @[ UIColor.whiteColor ];

  _statusLabel.numberOfLines = 1;
  _statusLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _statusLabel.textColor = [UIColor whiteColor];
  _statusLabel.textAlignment = NSTextAlignmentCenter;

  _pinEntryView.hidden = YES;
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];

  _iconView.frame = CGRectMake(0, 0, kIconRadius * 2, kIconRadius * 2.f);
  _iconView.center =
      CGPointMake(self.view.center.x, self.view.center.y + kCenterShift);

  [_activityIndicator sizeToFit];
  _activityIndicator.center = _iconView.center;

  _statusLabel.frame =
      CGRectMake(kMargin, _activityIndicator.center.y + kIconRadius + kPadding,
                 self.view.frame.size.width - kMargin * 2.f,
                 _statusLabel.font.pointSize * _statusLabel.numberOfLines);

  _pinEntryView.frame = CGRectMake(
      (self.view.frame.size.width - kPinEntryViewWidth) / 2.f,
      _statusLabel.frame.origin.y + _statusLabel.frame.size.height + kPadding,
      kPinEntryViewWidth, kPinEntryViewHeight);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.navigationController setNavigationBarHidden:YES animated:animated];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(hostSessionStatusChanged:)
             name:kHostSessionStatusChanged
           object:nil];
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
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (BOOL)prefersStatusBarHidden {
  return YES;
}

#pragma mark - Keyboard

- (void)keyboardWillShow:(NSNotification*)notification {
  CGSize keyboardSize =
      [[[notification userInfo] objectForKey:UIKeyboardFrameEndUserInfoKey]
          CGRectValue]
          .size;

  [UIView
      animateWithDuration:kKeyboardAnimationTime
               animations:^{
                 CGRect f = self.view.frame;
                 CGFloat newHeight =
                     self.view.frame.size.height - keyboardSize.height;
                 CGFloat overlap =
                     newHeight - (_pinEntryView.frame.origin.y +
                                  _pinEntryView.frame.size.height + kPadding);
                 if (overlap < 0) {
                   f.origin.y = overlap;
                   // TODO(yuweih): This may push the navigation bar off screen.
                   self.view.frame = f;
                 }
               }];
}

- (void)keyboardWillHide:(NSNotification*)notification {
  [UIView animateWithDuration:kKeyboardAnimationTime
                   animations:^{
                     CGRect f = self.view.frame;
                     f.origin.y = 0.f;
                     self.view.frame = f;
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
    case ClientViewClosed:
      [self dismissViewControllerAnimated:YES completion:nil];
      break;
  }
}

#pragma mark - Private

- (void)showConnectingState {
  [_pinEntryView endEditing:YES];
  _statusLabel.text =
      [NSString stringWithFormat:@"Connecting to %@", _remoteHostName];
  [_activityIndicator stopAnimating];
  _activityIndicator.cycleColors = @[ [UIColor whiteColor] ];
  _activityIndicator.indicatorMode = MDCActivityIndicatorModeIndeterminate;
  _activityIndicator.hidden = NO;
  _pinEntryView.hidden = YES;
  [_activityIndicator startAnimating];
}

- (void)showPinPromptState {
  _statusLabel.text = [NSString stringWithFormat:@"%@", _remoteHostName];
  [_activityIndicator stopAnimating];
  _activityIndicator.hidden = YES;
  _pinEntryView.hidden = NO;

  // TODO(yuweih): This may be called before viewDidAppear and miss the keyboard
  // callback.
  [_pinEntryView becomeFirstResponder];
}

- (void)showConnectedState {
  [_pinEntryView endEditing:YES];
  _statusLabel.text =
      [NSString stringWithFormat:@"Connected to %@", _remoteHostName];
  _activityIndicator.progress = 0.0;
  _pinEntryView.hidden = YES;
  _activityIndicator.hidden = NO;
  _activityIndicator.indicatorMode = MDCActivityIndicatorModeDeterminate;
  _activityIndicator.cycleColors = @[ [UIColor greenColor] ];
  [_activityIndicator startAnimating];
  _activityIndicator.progress = 1.0;

  HostViewController* hostViewController =
      [[HostViewController alloc] initWithClient:_client];
  _client = nil;

  // Replaces current (topmost) view controller with |hostViewController|.
  NSMutableArray* controllers =
      [self.navigationController.viewControllers mutableCopy];
  [controllers removeLastObject];
  [controllers addObject:hostViewController];
  [self.navigationController setViewControllers:controllers animated:NO];
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
    // TODO(nicholss): Implement an error screen.
    case SessionClosed:
      state = ClientViewClosed;
      break;
    default:
      LOG(ERROR) << "Unknown State for Session, " << sessionDetails.state;
      return;
  }
  [[NSOperationQueue mainQueue] addOperationWithBlock:^{
    [self setState:state];
  }];
}

@end
