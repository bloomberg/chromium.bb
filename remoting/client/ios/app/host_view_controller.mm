// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/app/host_view_controller.h"

#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "remoting/client/ios/session/remoting_client.h"

static const CGFloat kFabInset = 15.f;

@interface HostViewController () {
  RemotingClient* _client;
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

- (void)viewDidLoad {
  [super viewDidLoad];
  MDCFloatingButton* floatingButton =
      [MDCFloatingButton floatingButtonWithShape:MDCFloatingButtonShapeMini];
  [floatingButton setTitle:@"+" forState:UIControlStateNormal];
  [floatingButton addTarget:self
                     action:@selector(didTap:)
           forControlEvents:UIControlEventTouchUpInside];

  UIImage* settingsImage = [UIImage imageNamed:@"Settings"];
  [floatingButton setImage:settingsImage forState:UIControlStateNormal];
  [floatingButton sizeToFit];
  CGSize btnSize = floatingButton.frame.size;
  floatingButton.frame =
      CGRectMake(self.view.frame.size.width - btnSize.width - kFabInset,
                 self.view.frame.size.height - btnSize.height - kFabInset,
                 btnSize.width, btnSize.height);

  [self.view addSubview:floatingButton];
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
  GLKView* view = (GLKView*)self.view;
  view.context = [_client.displayHandler GetEAGLContext];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
}

#pragma mark - GLKViewDelegate

- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect {
  // Nothing to do that is synchronous yet.
}

#pragma mark - Private

- (void)didTap:(id)sender {
  // TODO(nicholss): The FAB is being used to close the window at the moment
  // just as a demo  as the integration continues. This will not be the case
  // in the final app.
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
