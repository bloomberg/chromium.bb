// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/host_view_controller.h"

#include <memory>

#import <GLKit/GLKit.h>

#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "remoting/ios/client_gestures.h"
#import "remoting/ios/client_keyboard.h"
#import "remoting/ios/session/remoting_client.h"

#include "remoting/client/gesture_interpreter.h"

static const CGFloat kFabInset = 15.f;
static const CGFloat kKeyboardAnimationTime = 0.3;

@interface HostViewController () {
  RemotingClient* _client;
  MDCFloatingButton* _floatingButton;
  ClientGestures* _clientGestures;
  ClientKeyboard* _clientKeyboard;
  CGSize _keyboardSize;
}
@end

@implementation HostViewController

- (id)initWithClient:(RemotingClient*)client {
  self = [super init];
  if (self) {
    _client = client;
    _keyboardSize = CGSizeZero;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  self.view = [[GLKView alloc] initWithFrame:CGRectZero];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  _floatingButton =
      [MDCFloatingButton floatingButtonWithShape:MDCFloatingButtonShapeMini];
  [_floatingButton setTitle:@"+" forState:UIControlStateNormal];
  [_floatingButton addTarget:self
                      action:@selector(didTap:)
            forControlEvents:UIControlEventTouchUpInside];

  UIImage* settingsImage = [UIImage imageNamed:@"Settings"];
  [_floatingButton setImage:settingsImage forState:UIControlStateNormal];
  [_floatingButton sizeToFit];
  [self.view addSubview:_floatingButton];
}

- (void)viewDidUnload {
  [super viewDidUnload];
  // TODO(nicholss): There needs to be a hook to tell the client we are done.
}

- (BOOL)prefersStatusBarHidden {
  return YES;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  GLKView* glView = (GLKView*)self.view;
  glView.context = [_client.displayHandler GetEAGLContext];
  [_client.displayHandler onSurfaceCreated:glView];

  // viewDidLayoutSubviews may be called before viewDidAppear, in which case
  // the surface is not ready to handle the transformation matrix.
  // Call onSurfaceChanged here to cover that case.
  [_client surfaceChanged:self.view.frame];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [(GLKView*)self.view deleteDrawable];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  _clientGestures =
      [[ClientGestures alloc] initWithView:self.view client:_client];
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

  _clientGestures = nil;
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  if (((GLKView*)self.view).context != nil) {
    // If the context is not set yet, the view size will be set in
    // viewDidAppear.
    [_client surfaceChanged:self.view.bounds];
  }

  CGSize btnSize = _floatingButton.frame.size;
  _floatingButton.frame =
      CGRectMake(self.view.frame.size.width - btnSize.width - kFabInset,
                 self.view.frame.size.height - btnSize.height - kFabInset,
                 btnSize.width, btnSize.height);
}

#pragma mark - Keyboard

- (BOOL)isKeyboardActive {
  if (_clientKeyboard) {
    return [_clientKeyboard isFirstResponder];
  }
  return NO;
}

- (void)showKeyboard {
  if (!_clientKeyboard) {
    _clientKeyboard = [[ClientKeyboard alloc] init];
    [self.view addSubview:_clientKeyboard];
    // TODO(nicholss): need to pass some keyboard injection interface here.
  }
  [_clientKeyboard becomeFirstResponder];
}

- (void)hideKeyboard {
  [_clientKeyboard resignFirstResponder];
}

#pragma mark - Keyboard Notifications

- (void)keyboardWillShow:(NSNotification*)notification {
  CGSize keyboardSize =
      [[[notification userInfo] objectForKey:UIKeyboardFrameBeginUserInfoKey]
          CGRectValue]
          .size;
  if (_keyboardSize.height != keyboardSize.height) {
    CGFloat deltaHeight = keyboardSize.height - _keyboardSize.height;
    [UIView animateWithDuration:0.3
                     animations:^{
                       CGRect f = self.view.frame;
                       f.size.height += deltaHeight;
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

#pragma mark - Private

- (void)didTap:(id)sender {
  // TODO(nicholss): The FAB is being used to launch an alert window with
  // more options. This is not ideal but it gets us an easy way to make a
  // modal window option selector. Replace this with a real menu later.

  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:@"Remote Settings"
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];

  if ([self isKeyboardActive]) {
    void (^hideKeyboardHandler)(UIAlertAction*) = ^(UIAlertAction*) {
      NSLog(@"Will hide keyboard.");
      [self hideKeyboard];
    };
    [alert addAction:[UIAlertAction actionWithTitle:@"Hide Keyboard"
                                              style:UIAlertActionStyleDefault
                                            handler:hideKeyboardHandler]];
  } else {
    void (^showKeyboardHandler)(UIAlertAction*) = ^(UIAlertAction*) {
      NSLog(@"Will show keyboard.");
      [self showKeyboard];
    };
    [alert addAction:[UIAlertAction actionWithTitle:@"Show Keyboard"
                                              style:UIAlertActionStyleDefault
                                            handler:showKeyboardHandler]];
  }
  void (^disconnectHandler)(UIAlertAction*) = ^(UIAlertAction*) {
    [_client disconnectFromHost];
    [self dismissViewControllerAnimated:YES completion:nil];
  };
  [alert addAction:[UIAlertAction actionWithTitle:@"Disconnect"
                                            style:UIAlertActionStyleCancel
                                          handler:disconnectHandler]];

  [self presentViewController:alert animated:YES completion:nil];
}

@end
