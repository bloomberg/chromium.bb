// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/host_view_controller.h"

#include <memory>

#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "remoting/ios/app/physical_keyboard_detector.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/settings/remoting_settings_view_controller.h"
#import "remoting/ios/app/view_utils.h"
#import "remoting/ios/client_gestures.h"
#import "remoting/ios/client_keyboard.h"
#import "remoting/ios/display/eagl_view.h"
#import "remoting/ios/domain/host_info.h"
#import "remoting/ios/domain/host_settings.h"
#import "remoting/ios/mdc/MDCActionImageView.h"
#import "remoting/ios/persistence/remoting_preferences.h"
#import "remoting/ios/session/remoting_client.h"

#include "base/strings/sys_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/gesture_interpreter.h"
#include "remoting/client/input/keyboard_interpreter.h"
#include "ui/base/l10n/l10n_util.h"

static const CGFloat kFabInset = 15.f;
static const CGFloat kKeyboardAnimationTime = 0.3;
static const CGFloat kMoveFABAnimationTime = 0.3;

@interface HostViewController ()<ClientKeyboardDelegate,
                                 ClientGesturesDelegate,
                                 RemotingSettingsViewControllerDelegate> {
  RemotingClient* _client;
  MDCActionImageView* _actionImageView;
  MDCFloatingButton* _floatingButton;
  ClientGestures* _clientGestures;
  ClientKeyboard* _clientKeyboard;
  CGSize _keyboardSize;
  BOOL _surfaceCreated;
  HostSettings* _settings;

  // Only change this by calling setFabIsRight:.
  BOOL _fabIsRight;
  NSArray<NSLayoutConstraint*>* _fabLeftConstraints;
  NSArray<NSLayoutConstraint*>* _fabRightConstraints;
  // When set to true, ClientKeyboard will immediately resign first responder
  // after it becomes first responder.
  BOOL _blocksKeyboard;
  BOOL _hasPhysicalKeyboard;
  NSLayoutConstraint* _keyboardHeightConstraint;

  // Subview of self.view. Adjusted frame for safe area.
  EAGLView* _hostView;

  // A placeholder view for anchoring views and calculating visible area.
  UIView* _keyboardPlaceholderView;

  // A display link for animating host surface size change. Use the paused
  // property to start or stop the animation.
  CADisplayLink* _surfaceSizeAnimationLink;
}
@end

@implementation HostViewController

- (id)initWithClient:(RemotingClient*)client {
  self = [super init];
  if (self) {
    _client = client;
    _keyboardSize = CGSizeZero;
    _surfaceCreated = NO;
    _blocksKeyboard = NO;
    _hasPhysicalKeyboard = NO;
    _settings =
        [[RemotingPreferences instance] settingsForHost:client.hostInfo.hostId];
    _surfaceSizeAnimationLink = [CADisplayLink
        displayLinkWithTarget:self
                     selector:@selector(animateHostSurfaceSize:)];
    _surfaceSizeAnimationLink.paused = YES;
    [_surfaceSizeAnimationLink addToRunLoop:NSRunLoop.currentRunLoop
                                    forMode:NSDefaultRunLoopMode];

    BOOL fabIsRight =
        [UIView userInterfaceLayoutDirectionForSemanticContentAttribute:
                    self.view.semanticContentAttribute] ==
        UIUserInterfaceLayoutDirectionLeftToRight;
    [self setFabIsRight:fabIsRight shouldLayout:NO];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  _hostView = [[EAGLView alloc] initWithFrame:CGRectZero];
  _hostView.translatesAutoresizingMaskIntoConstraints = NO;

  // Allow the host view to handle raw gestures.
  _hostView.isAccessibilityElement = YES;
  _hostView.accessibilityTraits = UIAccessibilityTraitAllowsDirectInteraction;
  [self.view addSubview:_hostView];

  UILayoutGuide* safeAreaLayoutGuide =
      remoting::SafeAreaLayoutGuideForView(self.view);

  [NSLayoutConstraint activateConstraints:@[
    [_hostView.topAnchor constraintEqualToAnchor:safeAreaLayoutGuide.topAnchor],
    [_hostView.bottomAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.bottomAnchor],
    [_hostView.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor],
    [_hostView.trailingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.trailingAnchor],
  ]];

  _hostView.displayTaskRunner =
      remoting::ChromotingClientRuntime::GetInstance()->display_task_runner();

  _keyboardPlaceholderView = [[UIView alloc] initWithFrame:CGRectZero];
  _keyboardPlaceholderView.translatesAutoresizingMaskIntoConstraints = NO;
  [_hostView addSubview:_keyboardPlaceholderView];
  [NSLayoutConstraint activateConstraints:@[
    [_keyboardPlaceholderView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_keyboardPlaceholderView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_keyboardPlaceholderView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
  ]];

  _floatingButton =
      [MDCFloatingButton floatingButtonWithShape:MDCFloatingButtonShapeMini];
  // Note(nicholss): Setting title to " " because the FAB requires the title
  // or image to be set but we are using the rotating image instead. Until this
  // is directly supported by the FAB, a space for the title is a work-around.
  [_floatingButton setTitle:@" " forState:UIControlStateNormal];
  [_floatingButton setBackgroundColor:RemotingTheme.buttonBackgroundColor
                             forState:UIControlStateNormal];
  [_floatingButton addTarget:self
                      action:@selector(didTap:)
            forControlEvents:UIControlEventTouchUpInside];
  [_floatingButton sizeToFit];
  _floatingButton.translatesAutoresizingMaskIntoConstraints = NO;

  _actionImageView =
      [[MDCActionImageView alloc] initWithFrame:_floatingButton.bounds
                                   primaryImage:RemotingTheme.menuIcon
                                    activeImage:RemotingTheme.closeIcon];
  [_floatingButton addSubview:_actionImageView];
  // TODO(yuweih): The accessibility label should be changed to "Close" when
  // the FAB is open.
  _floatingButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACTIONBAR_MENU);
  [self.view addSubview:_floatingButton];

  [self applyInputMode];

  _clientKeyboard = [[ClientKeyboard alloc] init];
  _clientKeyboard.delegate = self;
  [_hostView addSubview:_clientKeyboard];

  _fabLeftConstraints = @[ [_floatingButton.leftAnchor
      constraintEqualToAnchor:_hostView.leftAnchor
                     constant:kFabInset] ];
  _fabRightConstraints = @[ [_floatingButton.rightAnchor
      constraintEqualToAnchor:_hostView.rightAnchor
                     constant:-kFabInset] ];
  [_floatingButton.bottomAnchor
      constraintEqualToAnchor:_keyboardPlaceholderView.topAnchor
                     constant:-kFabInset]
      .active = YES;

  [self setKeyboardSize:CGSizeZero needsLayout:NO];

  remoting::PostDelayedAccessibilityNotification(
      l10n_util::GetNSString(IDS_HOST_CONNECTED_ANNOUNCEMENT));
}

- (void)viewDidUnload {
  [super viewDidUnload];
  // TODO(nicholss): There needs to be a hook to tell the client we are done.

  [_hostView stop];
  _clientGestures = nil;
  _client = nil;
}

- (BOOL)prefersStatusBarHidden {
  return YES;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  if (!_surfaceCreated) {
    [_client.displayHandler onSurfaceCreated:_hostView];
    _surfaceCreated = YES;
  }

  [PhysicalKeyboardDetector detectOnView:_hostView
                                callback:^(BOOL hasPhysicalKeyboard) {
                                  _hasPhysicalKeyboard = hasPhysicalKeyboard;
                                  [_clientKeyboard becomeFirstResponder];
                                }];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  if (!_clientGestures) {
    _clientGestures =
        [[ClientGestures alloc] initWithView:_hostView client:_client];
    _clientGestures.delegate = self;
  }
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
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];

  [[RemotingPreferences instance] setSettings:_settings
                                      forHost:_client.hostInfo.hostId];

  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // Pass the actual size of the view to the renderer.
  [_client.displayHandler onSurfaceChanged:_hostView.bounds];

  // Start the animation on the host's visible area.
  _surfaceSizeAnimationLink.paused = NO;

  [self resizeHostToFitIfNeeded];
}

#pragma mark - Keyboard Notifications

- (void)keyboardWillShow:(NSNotification*)notification {
  // The soft keyboard can be triggered by the PhysicalKeyboardDetector, in this
  // case we don't need to change the keyboard size.
  if (!_clientKeyboard.isFirstResponder) {
    return;
  }

  if (_blocksKeyboard) {
    // This is to make sure the keyboard is removed from the responder chain.
    [_clientKeyboard removeFromSuperview];
    [self.view addSubview:_clientKeyboard];
    _clientKeyboard.showsSoftKeyboard = NO;
    [_clientKeyboard becomeFirstResponder];
    return;
  }

  CGSize keyboardSize =
      [[[notification userInfo] objectForKey:UIKeyboardFrameEndUserInfoKey]
          CGRectValue]
          .size;
  [self setKeyboardSize:keyboardSize needsLayout:YES];
}

- (void)keyboardWillHide:(NSNotification*)notification {
  if (!_clientKeyboard.isFirstResponder) {
    return;
  }

  [self setKeyboardSize:CGSizeZero needsLayout:YES];
}

#pragma mark - ClientKeyboardDelegate

- (void)clientKeyboardShouldSend:(NSString*)text {
  _client.keyboardInterpreter->HandleTextEvent(base::SysNSStringToUTF8(text),
                                               0);
}

- (void)clientKeyboardShouldDelete {
  _client.keyboardInterpreter->HandleDeleteEvent(0);
}

#pragma mark - ClientGesturesDelegate

- (void)keyboardShouldShow {
  _clientKeyboard.showsSoftKeyboard = YES;
}

- (void)keyboardShouldHide {
  _clientKeyboard.showsSoftKeyboard = NO;
}

- (void)menuShouldShow {
  [self didTap:_floatingButton];
}

#pragma mark - RemotingSettingsViewControllerDelegate

- (void)setResizeToFit:(BOOL)resizeToFit {
  // TODO(yuweih): Maybe we add a native screen size mimimum before enabling
  // this option? This doesn't work well for smaller screens. Ask Jon.
  _settings.shouldResizeHostToFit = resizeToFit;
  [self resizeHostToFitIfNeeded];
}

- (void)useDirectInputMode {
  _settings.inputMode = ClientInputModeDirect;
  [self applyInputMode];
}

- (void)useTrackpadInputMode {
  _settings.inputMode = ClientInputModeTrackpad;
  [self applyInputMode];
}

- (void)sendCtrAltDel {
  _client.keyboardInterpreter->HandleCtrlAltDeleteEvent();
}

- (void)sendPrintScreen {
  _client.keyboardInterpreter->HandlePrintScreenEvent();
}

- (void)moveFAB {
  [self setFabIsRight:!_fabIsRight shouldLayout:YES];
}

#pragma mark - Private

- (void)setFabIsRight:(BOOL)fabIsRight shouldLayout:(BOOL)shouldLayout {
  _fabIsRight = fabIsRight;

  [NSLayoutConstraint deactivateConstraints:_fabRightConstraints];
  [NSLayoutConstraint deactivateConstraints:_fabLeftConstraints];
  if (_fabIsRight) {
    [NSLayoutConstraint activateConstraints:_fabRightConstraints];
  } else {
    [NSLayoutConstraint activateConstraints:_fabLeftConstraints];
  }

  if (shouldLayout) {
    [UIView animateWithDuration:kMoveFABAnimationTime
                     animations:^{
                       [self.view layoutIfNeeded];
                     }];
  }
}

- (void)resizeHostToFitIfNeeded {
  // Don't adjust the host resolution if the keyboard is active. That would end
  // up with a very narrow desktop.
  // Also don't adjust if it's the phone and in portrait orientation. This is
  // the most used orientation on phones but the aspect ratio is uncommon on
  // desktop devices.
  BOOL isPhonePortrait =
      self.traitCollection.horizontalSizeClass ==
          UIUserInterfaceSizeClassCompact &&
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular;

  if (_settings.shouldResizeHostToFit && !isPhonePortrait &&
      !_clientKeyboard.showsSoftKeyboard) {
    [_client setHostResolution:_hostView.frame.size
                         scale:_hostView.contentScaleFactor];
  }
}

- (void)animateHostSurfaceSize:(CADisplayLink*)link {
  // The method is called when the keyboard animation is in-progress. It
  // calculates the intermediate visible area size during the animation and
  // passes it to DesktopViewport.

  // This method is called in sync with the refresh cycle, otherwise the frame
  // rate will drop for some reason. Note that the actual rendering process is
  // done on the display thread asynchronously, so unfortunately the animation
  // will not be perfectly synchronized with the keyboard animation.

  CGSize viewSize = _hostView.frame.size;
  CGFloat targetVisibleHeight =
      viewSize.height - _keyboardPlaceholderView.frame.size.height;
  CALayer* kbPlaceholderLayer =
      [_keyboardPlaceholderView.layer presentationLayer];
  CGRect viewKeyboardIntersection =
      CGRectIntersection(kbPlaceholderLayer.frame, _hostView.frame);
  CGFloat currentVisibleHeight =
      _hostView.frame.size.height - viewKeyboardIntersection.size.height;
  _client.gestureInterpreter->OnSurfaceSizeChanged(viewSize.width,
                                                   currentVisibleHeight);
  if (currentVisibleHeight == targetVisibleHeight) {
    // Animation is done.
    _surfaceSizeAnimationLink.paused = YES;
  }
}

- (void)disconnectFromHost {
  [_client disconnectFromHost];
  [_surfaceSizeAnimationLink invalidate];
  _surfaceSizeAnimationLink = nil;
}

- (void)applyInputMode {
  switch (_settings.inputMode) {
    case ClientInputModeTrackpad:
      _client.gestureInterpreter->SetInputMode(
          remoting::GestureInterpreter::TRACKPAD_INPUT_MODE);
      break;
    case ClientInputModeDirect:  // Fall-through.
    default:
      _client.gestureInterpreter->SetInputMode(
          remoting::GestureInterpreter::DIRECT_INPUT_MODE);
  }
}

- (void)didTap:(id)sender {
  // TODO(nicholss): The FAB is being used to launch an alert window with
  // more options. This is not ideal but it gets us an easy way to make a
  // modal window option selector. Replace this with a real menu later.

  // ClientKeyboard may gain first responder immediately after the alert is
  // dismissed. This will cause weird show-then-hide animation when hiding
  // keyboard on iPhone (iPad is unaffected since it shows the alert as popup).
  // The fix is to remove ClientKeyboard from the responder chain in
  // keyboardWillShow and manually show the keyboard again only when needed.

  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];

  __weak HostViewController* weakSelf = self;
  if (!_hasPhysicalKeyboard) {
    // These are only needed if the device has no physical keyboard.
    __weak ClientKeyboard* weakClientKeyboard = _clientKeyboard;
    if (_clientKeyboard.showsSoftKeyboard) {
      [self addActionToAlert:alert
                       title:IDS_HIDE_KEYBOARD
                       style:UIAlertActionStyleDefault
            restoresKeyboard:NO
                     handler:^() {
                       weakClientKeyboard.showsSoftKeyboard = NO;
                     }];
    } else {
      [self addActionToAlert:alert
                       title:IDS_SHOW_KEYBOARD
                     handler:^() {
                       weakClientKeyboard.showsSoftKeyboard = YES;
                     }];
    }
  }

  remoting::GestureInterpreter::InputMode currentInputMode =
      _client.gestureInterpreter->GetInputMode();
  int switchInputModeTitle =
      currentInputMode == remoting::GestureInterpreter::DIRECT_INPUT_MODE
          ? IDS_SELECT_TRACKPAD_MODE
          : IDS_SELECT_TOUCH_MODE;
  void (^switchInputModeHandler)() = ^() {
    switch (currentInputMode) {
      case remoting::GestureInterpreter::DIRECT_INPUT_MODE:
        [self useTrackpadInputMode];
        break;
      case remoting::GestureInterpreter::TRACKPAD_INPUT_MODE:  // Fall-through.
      default:
        [self useDirectInputMode];
        break;
    }
  };
  [self addActionToAlert:alert
                   title:switchInputModeTitle
                 handler:switchInputModeHandler];

  void (^disconnectHandler)() = ^() {
    [weakSelf disconnectFromHost];
    [weakSelf.navigationController popToRootViewControllerAnimated:YES];
  };
  [self addActionToAlert:alert
                   title:IDS_DISCONNECT_MYSELF_BUTTON
                   style:UIAlertActionStyleDefault
        restoresKeyboard:NO
                 handler:disconnectHandler];

  void (^settingsHandler)() = ^() {
    RemotingSettingsViewController* settingsViewController =
        [[RemotingSettingsViewController alloc] init];
    settingsViewController.delegate = weakSelf;
    settingsViewController.inputMode = currentInputMode;
    settingsViewController.shouldResizeHostToFit =
        _settings.shouldResizeHostToFit;
    UINavigationController* navController = [[UINavigationController alloc]
        initWithRootViewController:settingsViewController];
    [weakSelf presentViewController:navController animated:YES completion:nil];
  };
  // Don't restore keyboard since the settings view will be show immediately.
  [self addActionToAlert:alert
                   title:IDS_SETTINGS_BUTTON
                   style:UIAlertActionStyleDefault
        restoresKeyboard:NO
                 handler:settingsHandler];

  [self addActionToAlert:alert
                   title:(_fabIsRight) ? IDS_MOVE_FAB_LEFT_BUTTON
                                       : IDS_MOVE_FAB_RIGHT_BUTTON
                 handler:^() {
                   [weakSelf moveFAB];
                 }];

  __weak UIAlertController* weakAlert = alert;
  void (^cancelHandler)() = ^() {
    [weakAlert dismissViewControllerAnimated:YES completion:nil];
  };
  [self addActionToAlert:alert
                   title:IDS_CANCEL
                   style:UIAlertActionStyleCancel
        restoresKeyboard:YES
                 handler:cancelHandler];

  alert.popoverPresentationController.sourceView = _hostView;
  // Target the alert menu at the top middle of the FAB.
  alert.popoverPresentationController.sourceRect = CGRectMake(
      _floatingButton.center.x, _floatingButton.frame.origin.y, 1.0, 1.0);

  alert.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionDown;
  [self presentViewController:alert animated:YES completion:nil];

  // Prevent keyboard from showing between (alert is shown, action is executed).
  _blocksKeyboard = YES;

  [_actionImageView setActive:YES animated:YES];
}

// Adds an action to the alert. And restores the states for you.
// restoresKeyboard:
//   Set to YES to show the keyboard if it was previously shown. Do not assume
//   the keyboard will always be hidden when the alert view is shown.
- (void)addActionToAlert:(UIAlertController*)alert
                   title:(int)titleMessageId
                   style:(UIAlertActionStyle)style
        restoresKeyboard:(BOOL)restoresKeyboard
                 handler:(void (^)())handler {
  BOOL isKeyboardActive = _clientKeyboard.showsSoftKeyboard;
  [alert addAction:[UIAlertAction
                       actionWithTitle:l10n_util::GetNSString(titleMessageId)
                                 style:style
                               handler:^(UIAlertAction*) {
                                 _blocksKeyboard = NO;
                                 if (isKeyboardActive && restoresKeyboard) {
                                   _clientKeyboard.showsSoftKeyboard = YES;
                                 }
                                 if (handler) {
                                   handler();
                                 }
                                 [_actionImageView setActive:NO animated:YES];
                               }]];
}

// Shorter version of addActionToAlert with default action style and
// restoresKeyboard == YES.
- (void)addActionToAlert:(UIAlertController*)alert
                   title:(int)titleMessageId
                 handler:(void (^)())handler {
  [self addActionToAlert:alert
                   title:titleMessageId
                   style:UIAlertActionStyleDefault
        restoresKeyboard:YES
                 handler:handler];
}

- (void)setKeyboardSize:(CGSize)keyboardSize needsLayout:(BOOL)needsLayout {
  _keyboardSize = keyboardSize;
  if (_keyboardHeightConstraint) {
    _keyboardHeightConstraint.active = NO;
  }
  _keyboardHeightConstraint = [_keyboardPlaceholderView.heightAnchor
      constraintEqualToConstant:keyboardSize.height];
  _keyboardHeightConstraint.active = YES;

  if (needsLayout) {
    [UIView animateWithDuration:kKeyboardAnimationTime
                     animations:^{
                       [self.view layoutIfNeeded];
                     }];
  }
}

@end
