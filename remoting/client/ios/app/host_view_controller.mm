// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/app/host_view_controller.h"

#import <GLKit/GLKit.h>

#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "remoting/client/ios/session/remoting_client.h"

static const CGFloat kFabInset = 15.f;

@interface HostViewController () {
  RemotingClient* _client;
  MDCFloatingButton* _floatingButton;
}
@end

@implementation HostViewController

- (id)initWithClient:(RemotingClient*)client {
  self = [super init];
  if (self) {
    _client = client;
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
  // the surface is not ready and onSurfaceChanged will be no-op.
  // Call onSurfaceChanged here to cover that case.
  [_client.displayHandler onSurfaceChanged:glView.frame];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  ((GLKView*)self.view).enableSetNeedsDisplay = true;
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  [_client.displayHandler onSurfaceChanged:((GLKView*)self.view).frame];

  const CGSize& btnSize = _floatingButton.frame.size;
  _floatingButton.frame =
      CGRectMake(self.view.frame.size.width - btnSize.width - kFabInset,
                 self.view.frame.size.height - btnSize.height - kFabInset,
                 btnSize.width, btnSize.height);
}

#pragma mark - Private

- (void)didTap:(id)sender {
  // TODO(nicholss): The FAB is being used to close the window at the moment
  // just as a demo  as the integration continues. This will not be the case
  // in the final app.
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
