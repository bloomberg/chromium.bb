// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/app/example_view_controller.h"

#import "remoting/client/display/sys_opengl.h"

#include "base/message_loop/message_loop.h"
#include "remoting/protocol/frame_stats.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

@interface ExampleViewController ()

// Helper functions dealing with the GL Context.
- (void)setupGL;
- (void)tearDownGL;

@end

@implementation ExampleViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  _gestures = [[ClientGestures alloc] initWithView:self.view];
  // TODO(nicholss): For prod code, make sure to check for ES3 support and
  // fall back to ES2 if needed.
  _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
  _runtime = new remoting::ios::AppRuntime();
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
  _display_handler = [[GlDisplayHandler alloc] initWithRuntime:_runtime];
  [_display_handler created];
}

- (void)tearDownGL {
  // TODO(nicholss): Implement this in a real application.
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
}

- (void)viewDidAppear:(BOOL)animated {
  _video_renderer =
      (remoting::SoftwareVideoRenderer*)[_display_handler CreateVideoRenderer]
          .get();
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
  if (_display_handler) {
    [_display_handler glkView:view drawInRect:rect];
  }
}

@end
