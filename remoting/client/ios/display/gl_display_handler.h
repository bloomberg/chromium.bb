// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_
#define REMOTING_CLIENT_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_

#import "remoting/client/display/sys_opengl.h"
#include "remoting/client/client_context.h"
#include "remoting/protocol/frame_consumer.h"
#include "remoting/protocol/video_renderer.h"

namespace remoting {
namespace ios {

class AppRuntime;

}  // namespace ios
}  // namespace remoting

@interface GlDisplayHandler : NSObject {
}

- (id)initWithRuntime:(remoting::ios::AppRuntime*)runtime;

- (void)created;
- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect;
- (std::unique_ptr<remoting::protocol::VideoRenderer>)CreateVideoRenderer;

@end

#endif  // REMOTING_CLIENT_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_
