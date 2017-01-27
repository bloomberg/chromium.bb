// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_APP_EXAMPLE_VIEW_CONTROLLER_H_
#define REMOTING_CLIENT_IOS_APP_EXAMPLE_VIEW_CONTROLLER_H_

#import <GLKit/GLKit.h>
#import "remoting/client/ios/display/gl_display_handler.h"
#include "remoting/client/ios/app_runtime.h"

#include "remoting/client/software_video_renderer.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/proto/video.pb.h"
#import "remoting/client/ios/client_gestures.h"

@interface ExampleViewController : GLKViewController {
 @private

  // The GLES3 context being drawn to.
  EAGLContext* _context;
  remoting::ios::AppRuntime* _runtime;
  GlDisplayHandler* _display_handler;
  remoting::SoftwareVideoRenderer* _video_renderer;

  remoting::VideoEncoderVerbatim _encoder;
  ClientGestures* _gestures;
}

@end

#endif  // REMOTING_CLIENT_IOS_APP_EXAMPLE_VIEW_CONTROLLER_H_
