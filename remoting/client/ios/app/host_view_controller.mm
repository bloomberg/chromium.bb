// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/app/host_view_controller.h"

#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "remoting/client/display/sys_opengl.h"
#import "remoting/client/ios/display/gl_display_handler.h"
#import "remoting/client/ios/facade/remoting_service.h"

#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/software_video_renderer.h"

static const CGFloat kFabInset = 45.f;

@interface HostViewController () {
  EAGLContext* _context;
  GlDisplayHandler* _display_handler;
  remoting::SoftwareVideoRenderer* _video_renderer;
}
@end

@implementation HostViewController

- (id)init {
  self = [super init];
  if (self) {
    // TODO(nicholss): For prod code, make sure to check for ES3 support and
    // fall back to ES2 if needed.
    _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  // TODO(nicholss): Take a look at the following:
  // components/Snackbar/examples/SnackbarOverlayViewExample.m
  // to add the toast overlay.
  MDCFloatingButton* floatingButton = [MDCFloatingButton new];
  [floatingButton setTitle:@"+" forState:UIControlStateNormal];
  [floatingButton sizeToFit];
  [floatingButton addTarget:self
                     action:@selector(didTap:)
           forControlEvents:UIControlEventTouchUpInside];

  UIImage* settingsImage = [UIImage imageNamed:@"Settings"];
  [floatingButton setImage:settingsImage forState:UIControlStateNormal];

  floatingButton.frame = CGRectMake(
      self.view.frame.size.width - floatingButton.frame.size.width - kFabInset,
      self.view.frame.size.height - floatingButton.frame.size.height -
          kFabInset,
      floatingButton.frame.size.width, floatingButton.frame.size.height);

  [self.view addSubview:floatingButton];

  static_cast<GLKView*>(self.view).context = _context;
  [self setupGL];
}

- (void)viewDidUnload {
  [super viewDidUnload];
  [self tearDownGL];

  if ([EAGLContext currentContext] == _context) {
    [EAGLContext setCurrentContext:nil];
  }
  _context = nil;
}

- (BOOL)prefersStatusBarHidden {
  return YES;
}

- (void)viewDidAppear:(BOOL)animated {
  _video_renderer =
      (remoting::SoftwareVideoRenderer*)[_display_handler CreateVideoRenderer]
          .get();
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:NO];
}

#pragma mark - GLKViewDelegate

// In general, avoid expensive work in this function to maximize frame rate.
- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect {
  if (_display_handler) {
    [_display_handler glkView:view drawInRect:rect];
  }
}

#pragma mark - Private

- (void)setupGL {
  _display_handler = [[GlDisplayHandler alloc]
      initWithRuntime:[[RemotingService SharedInstance] runtime]];
  [_display_handler created];
}

- (void)tearDownGL {
  // TODO(nicholss): Implement tearDownGL for the real application.
  [_display_handler stop];
  _display_handler = nil;
  _video_renderer = nil;
}

- (void)didTap:(id)sender {
  // TODO(nicholss): The FAB is being used to close the window at the moment
  // just as a demo  as the integration continues. This will not be the case
  // in the final app.
  [self tearDownGL];
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
