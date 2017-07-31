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
}
@end

@implementation HostViewController

- (id)initWithClient:(RemotingClient*)client {
  self = [super init];
  if (self) {
    _client = client;
    _keyboardSize = CGSizeZero;
    _surfaceCreated = NO;
    _settings =
        [[RemotingPreferences instance] settingsForHost:client.hostInfo.hostId];
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  EAGLView* glView = [[EAGLView alloc] initWithFrame:CGRectZero];
  glView.displayTaskRunner =
      remoting::ChromotingClientRuntime::GetInstance()->display_task_runner();
  self.view = glView;
}

- (void)viewDidLoad {
  [super viewDidLoad];
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

  _actionImageView =
      [[MDCActionImageView alloc] initWithFrame:_floatingButton.bounds
                                   primaryImage:RemotingTheme.menuIcon
                                    activeImage:RemotingTheme.closeIcon];
  [_floatingButton addSubview:_actionImageView];
  [self.view addSubview:_floatingButton];

  [self applyInputMode];

  _clientKeyboard = [[ClientKeyboard alloc] init];
  _clientKeyboard.delegate = self;
  [self.view addSubview:_clientKeyboard];
  // TODO(nicholss): need to pass some keyboard injection interface here.
}

- (void)viewDidUnload {
  [super viewDidUnload];
  // TODO(nicholss): There needs to be a hook to tell the client we are done.

  [(EAGLView*)self.view stop];
  _clientGestures = nil;
  _client = nil;
}

- (BOOL)prefersStatusBarHidden {
  return YES;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  if (!_surfaceCreated) {
    [_client.displayHandler onSurfaceCreated:(EAGLView*)self.view];
    _surfaceCreated = YES;
  }
  // viewDidLayoutSubviews may be called before viewDidAppear, in which case
  // the surface is not ready to handle the transformation matrix.
  // Call onSurfaceChanged here to cover that case.
  [_client surfaceChanged:self.view.frame];
  [self resizeHostToFitIfNeeded];

  [PhysicalKeyboardDetector detectOnView:self.view
                                callback:^(BOOL hasPhysicalKeyboard) {
                                  if (hasPhysicalKeyboard) {
                                    _clientKeyboard.hasPhysicalKeyboard =
                                        hasPhysicalKeyboard;

                                    // Directly make the client keyboard first
                                    // responder so that it can immediately
                                    // handle key inputs.
                                    [self showKeyboard];
                                  }
                                }];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  if (!_clientGestures) {
    _clientGestures =
        [[ClientGestures alloc] initWithView:self.view client:_client];
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

  if (self.view.window != nil) {
    // If the context is not set yet, the view size will be set in
    // viewDidAppear.
    [_client surfaceChanged:self.view.bounds];
    [self resizeHostToFitIfNeeded];
  }

  CGSize btnSize = _floatingButton.frame.size;
  _floatingButton.frame =
      CGRectMake(self.view.frame.size.width - btnSize.width - kFabInset,
                 self.view.frame.size.height - btnSize.height - kFabInset,
                 btnSize.width, btnSize.height);
}

#pragma mark - Keyboard

- (BOOL)isKeyboardActive {
  return [_clientKeyboard isFirstResponder];
}

- (void)showKeyboard {
  [_clientKeyboard becomeFirstResponder];
}

- (void)hideKeyboard {
  [_clientKeyboard resignFirstResponder];
}

#pragma mark - Keyboard Notifications

- (void)keyboardWillShow:(NSNotification*)notification {
  CGSize keyboardSize =
      [[[notification userInfo] objectForKey:UIKeyboardFrameEndUserInfoKey]
          CGRectValue]
          .size;
  if (_keyboardSize.height != keyboardSize.height) {
    CGFloat deltaHeight = keyboardSize.height - _keyboardSize.height;
    [UIView animateWithDuration:kKeyboardAnimationTime
                     animations:^{
                       CGRect f = self.view.frame;
                       f.size.height -= deltaHeight;
                       self.view.frame = f;
                     }];
    _keyboardSize = keyboardSize;
  }
}

- (void)keyboardWillHide:(NSNotification*)notification {
  [UIView animateWithDuration:kKeyboardAnimationTime
                   animations:^{
                     CGRect f = self.view.frame;
                     f.size.height += _keyboardSize.height;
                     self.view.frame = f;
                   }];
  _keyboardSize = CGSizeZero;
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
  [self showKeyboard];
}

- (void)keyboardShouldHide {
  [self hideKeyboard];
}

#pragma mark - RemotingSettingsViewControllerDelegate

- (void)setShrinkToFit:(BOOL)shrinkToFit {
  // TODO(nicholss): I don't think this option makes sense for mobile.
  NSLog(@"TODO: shrinkToFit %d", shrinkToFit);
}

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

#pragma mark - Private

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
      ![self isKeyboardActive]) {
    [_client setHostResolution:self.view.frame.size
                         scale:self.view.contentScaleFactor];
  }
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

  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:nil
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];

  if (!_clientKeyboard.hasPhysicalKeyboard) {
    // These are only needed for soft keyboard.
    if ([self isKeyboardActive]) {
      void (^hideKeyboardHandler)(UIAlertAction*) = ^(UIAlertAction*) {
        [self hideKeyboard];
        [_actionImageView setActive:NO animated:YES];
      };
      [alert addAction:[UIAlertAction actionWithTitle:l10n_util::GetNSString(
                                                          IDS_HIDE_KEYBOARD)
                                                style:UIAlertActionStyleDefault
                                              handler:hideKeyboardHandler]];
    } else {
      void (^showKeyboardHandler)(UIAlertAction*) = ^(UIAlertAction*) {
        [self showKeyboard];
        [_actionImageView setActive:NO animated:YES];
      };
      [alert addAction:[UIAlertAction actionWithTitle:l10n_util::GetNSString(
                                                          IDS_SHOW_KEYBOARD)
                                                style:UIAlertActionStyleDefault
                                              handler:showKeyboardHandler]];
    }
  }

  remoting::GestureInterpreter::InputMode currentInputMode =
      _client.gestureInterpreter->GetInputMode();
  NSString* switchInputModeTitle = l10n_util::GetNSString(
      currentInputMode == remoting::GestureInterpreter::DIRECT_INPUT_MODE
          ? IDS_SELECT_TRACKPAD_MODE
          : IDS_SELECT_TOUCH_MODE);
  void (^switchInputModeHandler)(UIAlertAction*) = ^(UIAlertAction*) {
    switch (currentInputMode) {
      case remoting::GestureInterpreter::DIRECT_INPUT_MODE:
        [self useTrackpadInputMode];
        break;
      case remoting::GestureInterpreter::TRACKPAD_INPUT_MODE:  // Fall-through.
      default:
        [self useDirectInputMode];
        break;
    }
    [_actionImageView setActive:NO animated:YES];
  };
  [alert addAction:[UIAlertAction actionWithTitle:switchInputModeTitle
                                            style:UIAlertActionStyleDefault
                                          handler:switchInputModeHandler]];

  void (^disconnectHandler)(UIAlertAction*) = ^(UIAlertAction*) {
    [_client disconnectFromHost];
    [self.navigationController popToRootViewControllerAnimated:YES];
    [_actionImageView setActive:NO animated:YES];
  };
  [alert
      addAction:[UIAlertAction actionWithTitle:l10n_util::GetNSString(
                                                   IDS_DISCONNECT_MYSELF_BUTTON)
                                         style:UIAlertActionStyleDefault
                                       handler:disconnectHandler]];

  __weak HostViewController* weakSelf = self;
  void (^settingsHandler)(UIAlertAction*) = ^(UIAlertAction*) {
    RemotingSettingsViewController* settingsViewController =
        [[RemotingSettingsViewController alloc] init];
    settingsViewController.delegate = weakSelf;
    settingsViewController.inputMode = currentInputMode;
    settingsViewController.shouldResizeHostToFit =
        _settings.shouldResizeHostToFit;
    UINavigationController* navController = [[UINavigationController alloc]
        initWithRootViewController:settingsViewController];
    [weakSelf presentViewController:navController animated:YES completion:nil];
    [_actionImageView setActive:NO animated:YES];
  };
  [alert addAction:[UIAlertAction actionWithTitle:l10n_util::GetNSString(
                                                      IDS_SETTINGS_BUTTON)
                                            style:UIAlertActionStyleDefault
                                          handler:settingsHandler]];

  __weak UIAlertController* weakAlert = alert;
  void (^cancelHandler)(UIAlertAction*) = ^(UIAlertAction*) {
    [weakAlert dismissViewControllerAnimated:YES completion:nil];
    [_actionImageView setActive:NO animated:YES];
  };
  [alert addAction:[UIAlertAction
                       actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                 style:UIAlertActionStyleCancel
                               handler:cancelHandler]];

  alert.popoverPresentationController.sourceView = self.view;
  // Target the alert menu at the top middle of the FAB.
  alert.popoverPresentationController.sourceRect = CGRectMake(
      _floatingButton.center.x, _floatingButton.frame.origin.y, 1.0, 1.0);

  alert.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionDown;
  [self presentViewController:alert animated:YES completion:nil];
  [_actionImageView setActive:YES animated:YES];
}

@end
