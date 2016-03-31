// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/example_view_controller.h"

#import "remoting/client/opengl_wrapper.h"

@interface ExampleViewController()

// Helper functions dealing with the GL Context.
- (void)setupGL;
- (void)tearDownGL;

@end

@implementation ExampleViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
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

- (void)setupGL {
  [EAGLContext setCurrentContext:_context];
}

- (void)tearDownGL {
  [EAGLContext setCurrentContext:_context];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
}

- (void)viewDidAppear:(BOOL)animated {
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:NO];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
}

#pragma mark - GLKViewDelegate

// In general, avoid expensive work in this function to maximize frame rate.
- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect {
  // Clear to give the background color.
  glClearColor(0.0, 40.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
}

@end
